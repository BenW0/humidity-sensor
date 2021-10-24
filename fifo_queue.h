// A simple FIFO queue of fixed size for use by an arbitrary type

template <typename data_t, uint16_t size>
class FifoQueue
{
public:
  FifoQueue()
  {
    newest = queue;
    oldest = queue;
  }
  void Push(const data_t &entry)
  {
    *newest = entry;
    Increment(&newest);

    if (newest == oldest)
      Increment(&oldest);
  }
  bool Pop(data_t &result)
  {
    if (oldest == newest)
      return false;
    result = *oldest;
    Increment(&oldest);
    return true;
  }
  bool Peek(data_t &result)
  {
    if (oldest == newest)
      return false;
      result = *oldest;
      return true;
  }
  uint16_t Count() const
  {
    if (newest >= oldest)
      return newest - oldest;
    return size + 1 - (oldest - newest);
  }
  void Clear()
  {
    newest = queue;
    oldest = queue;
  }

private:
  void Increment(data_t **pt)
  {
    (*pt)++;
    if (*pt >= queue + size + 1)
      *pt = queue;
  }
  data_t queue[size + 1];
  data_t *newest;
  data_t *oldest;
};

/* Tests:

int main()
{
    FifoQueue<int, 4> q;
    int var{0};
    q.Push(4);
    std::cout << q.Count() << " expect 1" << endl;
    std::cout << q.Pop(var) << " " << var << " expect 4" << endl;
    q.Push(3);
    q.Push(2);
    q.Push(1);
    std::cout << q.Count() << " expect 3" << endl;
    std::cout << q.Pop(var) << " " << var << " expect 3" << endl;
    q.Push(0);
    q.Push(-1);
    std::cout << q.Count() << " expect 3" << endl;
    cout << q.Pop(var) << " " << var << "expect 1" << endl;
    q.Push(6);
    q.Push(7);
    cout << q.Count() << " expect 4" << endl;
    cout << q.Pop(var) << " " << var << "excpect 0" << endl;
    cout << q.Pop(var) << " " << var << "expect -1" << endl;
    cout << q.Pop(var) << " " << var << "excpet 6" << endl;
    cout << q.Pop(var) << " " << var << "expect 7" << endl;
    cout << q.Pop(var) << " " << var << "expect false" << endl;
    q.Push(8);
    q.Clear();
    cout << q.Count() << " expect 0" << endl;

    return 0;
}
*/