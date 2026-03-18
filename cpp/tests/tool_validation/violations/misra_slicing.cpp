// MISRA C++:2023: Object slicing shall not occur when passing a derived class
// object to a function accepting a base class by value.
// Expected diagnostic: cppcoreguidelines-slicing

class TvsSliceBase {
public:
    virtual ~TvsSliceBase() = default;
    int value = 0;
};

class TvsSliceDerived : public TvsSliceBase {
public:
    int extra = 42;
};

int tvs_accept_by_value(TvsSliceBase b) {  // Accepts base by value
    return b.value;
}

int tvs_trigger_slicing() {
    TvsSliceDerived d;
    return tvs_accept_by_value(d);  // Class slicing — target violation
}
