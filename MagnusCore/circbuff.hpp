// A simple circular buffer/queue structure
template <typename T, unsigned int BuffSize>
class CircularBuffer {
    public:
        CircularBuffer() : head(0), tail(0) {}
        void add(const T& t) {
            buf[head] = t;
            lastIdx_ = head;
            inc(head);
        }

        void remove() {
            if (head != tail) {
                inc(tail);
            }
        }

        T& peek() {
            return buf[tail];
        }

        // The index of the entry returned by 'peek'
        unsigned int currIdx() const { return tail; }

        // The index of the last entry added to the queue
        unsigned int lastIdx() const { return lastIdx_; }

        unsigned int size() {
            if (head >= tail) {
                return head - tail;
            } else {
                return head + BuffSize - tail;
            }
        }

        bool empty() {
            return head == tail;
        }

        void clear() {
            tail = head;
        }

        void setTo(const T& t) {
            clear();
            add(t);
        }

    private:
        void inc(unsigned int& x) {
            x++;
            if (x >= BuffSize) {
                x = 0;
            }
        }

        T buf[BuffSize];
        unsigned int head, tail, lastIdx_;
};
