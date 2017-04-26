// A simple circular buffer/queue structure
template <typename T>
class CircularBuffer {
    public:
        CircularBuffer() : head(0), tail(0) {}
        void add(const T& t) {
            buf[head] = t;
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
        inc(unsigned int& x) {
            x++;
            if (x >= BuffSize) {
                x = 0;
            }
        }

        static const unsigned int BuffSize = 5;
        T buf[BuffSize];
        unsigned int head, tail;
};
