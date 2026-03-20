// MISRA C++:2023 Rule 15.0.1 (Required): Special member functions shall be
// provided appropriately (Rule of Five).
// Expected diagnostic: cppcoreguidelines-special-member-functions

class TvsBuffer {
public:
  TvsBuffer() : data_(new int[10]) {}
  ~TvsBuffer() {
    delete[] data_;
  } // Destructor without copy/move — target violation
private:
  int* data_;
};
