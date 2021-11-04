
// TODO:
//  - Check for other sensors data which is out of date and alert if active.
//  - Send a weekly update email
//  - Summarize and archive when we get close to running out of rows


// Paste the URL of the Google Sheets starting from https thru /edit
// For e.g.: https://docs.google.com/..../edit 
const ssUrl = "https://docs.google.com/spreadsheets/d/.../edit";

// This method will be called first or hits first  
function doGet(e){
  Logger.log("--- doGet ---");
  
  // this helps during debuggin
  if (e == null){
    e={}; e.parameters = {name: "Hornet", date:new Date(),badValues:"1",minTemp:"40",meanTemp:"43",maxTemp:"101", minHumidity:"-3",meanHumidity:"51",maxHumidity:"55"};
    }

  try {
    var ss = SpreadsheetApp.openByUrl(ssUrl);
    var summarySheet = ss.getSheetByName("Summary");
    
    date = e.parameters.date;
    badValues = e.parameters.badValues;
    temps = [e.parameters.minTemp, e.parameters.meanTemp, e.parameters.maxTemp];
    humids = [e.parameters.minHumidity, e.parameters.meanHumidity, e.parameters.maxHumidity];
 
    // save the data to spreadsheet
    save_data(ss, e.parameters.name, date, temps, humids, badValues);

    SpreadsheetApp.flush();

    notify_violations(ss, e.parameters.name);

    send_summary(summarySheet);
 
    return ContentService.createTextOutput("Wrote:\n  " + date + "\n  Name:" + e.parameters.name);
 
  } catch(error) { 
    Logger.log(error);    
    return ContentService.createTextOutput("oops...." + error.message 
                                            + "\n" + new Date() 
                                            + "\n" + JSON.stringify(e.parameters));
  }  
}

// Gets called daily to make sure all the sensors are reporting
function do_timer(e)
{
  Logger.log("---- do_timer ----");
  var ss = SpreadsheetApp.openByUrl(ssUrl);
  SpreadsheetApp.flush();
  var summarySheet = ss.getSheetByName("Summary");
  
  var emailCell = summarySheet.getRange("EmailAddresses").offset(0, 1);
  var staleCell = summarySheet.getRange("SensorStaleEmail").offset(0, 1);
  var emailsToSend = {};
  var cellsTpUpdate = [];
  while (!summarySheet.getRange(1, emailCell.getColumn()).isBlank())
  {
    if (staleCell.getValue())
    {
      const sensorName = summarySheet.getRange("SummaryBlock").offset(1, emailCell.getColumn() - 1).getValue();
      const lastUpdate = summarySheet.getRange("LastUpdate").offset(0, emailCell.getColumn() - 1).getValue();
      const message = "Sensor '" + sensorName + "' hasn't reported data since " + String(lastUpdate);
      var theseEmails = String(emailCell.getValue()).split(';');
      for(var thisEmail of theseEmails)
      {
        thisEmail = thisEmail.trim();
        if (thisEmail == "")
          continue;
        if (!(thisEmail in emailsToSend))
          emailsToSend[thisEmail] = message;
        else
          emailsToSend[thisEmail] += "<br>" + message;
      }
      cellsTpUpdate.push(summarySheet.getRange("LastAlarm").offset(0, emailCell.getColumn() - 1));
    }
    emailCell = emailCell.offset(0, 1);
    staleCell = staleCell.offset(0, 1);
  }

  for (var email in emailsToSend)
  {
    try{
    Logger.log("Sending email to " + email + "\n" + emailsToSend[email]);
    MailApp.sendEmail({
        to: email,
        subject: "Humidity sensor not reporting",
        htmlBody: emailsToSend[email]
      });
    }
    catch(error) {
      Logger.log(JSON.stringify(error));
    }
  }

  const dateTime = new Date();
  for (var cell of cellsTpUpdate)
  {
    cell.setValue(dateTime);
  }
  Logger.log("Done");
}

function get_summary_column(summarySheet, sensor_name)
{
    // find the column for this sensor
    for (var i = 0; i < 30; ++i)
    {
      if (summarySheet.getRange(1, 2 + i).getValue() == sensor_name)
        return i+2;
    }
    return -1;
}
 
// Method to save given data to a sheet
function save_data(ss, sensor_name, date, temps, humids, badValues){
  try {
    var dateTime = new Date();
    var summarySheet = ss.getSheetByName("Summary");
    var dataLoggerSheet = ss.getSheetByName(sensor_name + " Data");

    // Insert a new row at the top of the sheet
    dataLoggerSheet.insertRowAfter(1)
    var row = 2;
 
    // Start Populating the data
    dataLoggerSheet.getRange(row, 1).setValue(dataLoggerSheet.getLastRow() + 1); // ID
    dataLoggerSheet.getRange(row, 2).setValue(dateTime); // dateTime
    dataLoggerSheet.getRange(row, 3).setValue(date); // tag
    for (var i = 0; i < 3; ++i)
      dataLoggerSheet.getRange(row, 4 + i).setValue(temps[i]);
    for (var i = 0 ; i < 3; ++i)
      dataLoggerSheet.getRange(row, 7 + i).setValue(humids[i]);
    dataLoggerSheet.getRange(row, 10).setValue(badValues);
    
    // Update summary sheet
    summary_col = get_summary_column(summarySheet, sensor_name);
    summarySheet.getRange(2, summary_col).setValue(dateTime); // Last modified date
  }
 
  catch(error) {
    Logger.log(JSON.stringify(error));
  }
}

function notify_violations(ss, sensor_name){
  var summarySheet = ss.getSheetByName("Summary");
  var summaryCol = get_summary_column(summarySheet, sensor_name);
  if (summaryCol < 0)
    return;
  var dateTime = new Date();

  // Do we need to send an email? Only if the last alarm is more than the specified number of days old

  if (!summarySheet.getRange("SendAlarmEmail").offset(0, summaryCol - 1).getValue())
    return;
  
  var emailAddresses = String(summarySheet.getRange("EmailAddresses").offset(0, summaryCol - 1).getValue());
  if (emailAddresses != null && emailAddresses != "")
  {
    Logger.log("--- sending alarm emails ---");
    var alarms = summarySheet.getRange("AlarmMessage").offset(0, summaryCol - 1).getValue();
    alarms = String(alarms).replaceAll("\\n", "<br>",);
    var message = "Sensor: " + sensor_name + "<br>" + alarms + "<br><br>Date : " + dateTime;
  
    var emails = emailAddresses.split(';');
    for(var email of emails)
    {
      email = email.trim();
      if(email != "")
      {
        
        Logger.log(" Sending summary to " + email);
        MailApp.sendEmail({
          to: email,
          subject: "Humidity sensor " + sensor_name + " Alarm",
          htmlBody: message
        });
      }
    }

    // record the last time we sent an email to avoid spamming ourselves
    summarySheet.getRange("LastAlarm").offset(0, summaryCol - 1).setValue(dateTime);
  }
  
}

/**
 * @param {SpreadsheetApp.Sheet} summarySheet 
 */
function build_summary_tables(summarySheet)
{
  // Return a dict of email address : corresponding html table,
  // showing only the columns to which that email address has subscribed

  /**
   * @param {Array} colFilter - a list of columns to include
   */
  function build_one_table(colFilter)
  {
    var startCell = summarySheet.getRange("SummaryBlock");
    var rng = startCell.offset(1, 0);
    

    var table = '<table style="border: 1px solid black; border-collapse: collapse;"><tr>';
    var colCount = 0;
    while(!rng.isBlank())
    {
      colCount++;
      if (colFilter.indexOf(colCount) !== -1)
        table += '<th style="border: 1px solid black;">' + String(rng.getValue()) + "</th>";
      rng = rng.offset(0, 1);
    }
    table += '</tr>'
    rng = rng.offset(1, -colCount);
    while(!rng.isBlank())
    {
      table += "<tr>"
      for(var i = 0; i < colCount; i++)
      {
        var value = String(rng.getValue());
        var color = String(rng.getBackground());
        if (colFilter.indexOf(i + 1) !== -1)
          table += '<td style="border: 1px solid black; background-color: ' + color + '"">' + value + '</td>';
        rng = rng.offset(0, 1);
      }
      table += "</tr>"
      rng = rng.offset(1, -colCount);
    }
    table += "</table>"
    return table;
  };

  var emailColFilters = {};
  var output = {};
  var emailCell = summarySheet.getRange("EmailAddresses").offset(0, 1);
  var plotTitlesRow = summarySheet.getRange("ChartTitles").getRow();
  while (!summarySheet.getRange(1, emailCell.getColumn()).isBlank())
  {
    var theseEmails = String(emailCell.getValue()).split(';');
    for(var thisEmail of theseEmails)
    {
      thisEmail = thisEmail.trim();
      if (thisEmail == "")
        continue;
      var plotTitle = summarySheet.getRange(plotTitlesRow, emailCell.getColumn()).getValue();
      if(!(thisEmail in emailColFilters))
      {
        emailColFilters[thisEmail] = [1, emailCell.getColumn()];
        output[thisEmail] = {plots:[plotTitle]};
      }
      else
      {
        emailColFilters[thisEmail].push(emailCell.getColumn());
        output[thisEmail].plots.push(plotTitle);
      }
    }
    emailCell = emailCell.offset(0, 1);
  }

  for(var email in emailColFilters)
    output[email].html_table = build_one_table(emailColFilters[email]);
  return output;
}

/**
 * @param {SpreadsheetApp.Sheet} summarySheet 
 */
function send_summary(summarySheet){
  var dateTime = new Date();

  // Has it been a week since we sent the last summary email?
  var needEmail = false;
  try
  {
    var ageThreshold = summarySheet.getRange("SummaryFrequency").getValue();
    var lastSummary = new Date(summarySheet.getRange("LastSummary").getValue());
    var nextSummary = new Date(lastSummary);
    nextSummary.setDate(nextSummary.getDate() + ageThreshold);
    needEmail = nextSummary < dateTime;
  }
  catch(error)
  {
    Logger.log("Error computing time since last summary email: " + JSON.stringify(error))
  }
  
  if (needEmail)
  {
    Logger.log("--- sending summary email ---");

    const summaries = build_summary_tables();

    // Get chart blobs ready
    const charts = summarySheet.getCharts();

    const chartDict = {};
    charts.forEach(function (chart, i) {
      chartDict[chart.getOptions().get('title')] = chart.getAs("image/png");
    });

    for(var email in summaries)
    {
      var message = "<b>Weekly humidy sensor summary email</b><br>";
      message += summaries[email].html_table;
      message += "<br><br>";

      // include only the plots this user is subscribed to
      const emailImages = {};

      var i = 0;
      for(var name in chartDict)
      {
        for(var needName of summaries[email].plots)
        {
          if(name.includes(needName) || name.includes("Everything") || name.includes("All"))
          {
            message += "<p align='center'><img src='cid:chart" + i + "'></p>";
            emailImages["chart" + i] = chartDict[name];
            i++;
            break;
          }
        }
      }

      Logger.log(" Sending summary to " + email);
      MailApp.sendEmail({
        to: email,
        subject: "Humidity sensor weekly summary",
        htmlBody: message,
        inlineImages: emailImages
      });
    }
    // record the last time we sent an email to avoid spamming ourselves
    summarySheet.getRange("LastSummary").setValue(dateTime);
  }
  else
  {
    Logger.log("No need to send summary emails");
  }
  
}