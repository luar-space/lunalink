// Power of Ten Rule 4 / JPL Rule 25: Functions shall have low cognitive
// complexity; deeply nested control flow shall be avoided.
// Expected diagnostic: readability-function-cognitive-complexity

int tvs_cognitive_complexity(int a, int b, int c, int d) {
    int result = 0;
    if (a > 0) {                        // +1
        if (b > 0) {                    // +2 (nesting)
            if (c > 0) {                // +3 (nesting)
                if (d > 0) {            // +4 (nesting)
                    result = 1;
                } else {                // +1
                    result = 2;
                }
            } else {                    // +1
                if (d > 0) {            // +4 (nesting)
                    result = 3;
                } else {                // +1
                    result = 4;
                }
            }
        } else {                        // +1
            if (c > 0) {                // +3 (nesting)
                result = 5;
            } else {                    // +1
                if (d > 0) {            // +4 (nesting)
                    result = 6;
                } else {                // +1
                    result = 7;
                }
            }
        }
    } else {                            // +1
        if (b > 0) {                    // +2 (nesting)
            result = 8;
        }
    }
    return result;
}
