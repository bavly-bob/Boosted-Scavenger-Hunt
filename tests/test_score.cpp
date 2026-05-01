#include "Coin.h"

#include <iostream>

int main()
{
    Coin coin(1, 2, 150);
    if (coin.isCollected()) {
        std::cerr << "Coin should start uncollected\n";
        return 1;
    }
    if (coin.getValue() != 150) {
        std::cerr << "Coin value mismatch\n";
        return 1;
    }

    coin.collect();
    if (!coin.isCollected()) {
        std::cerr << "Coin collect() did not update state\n";
        return 1;
    }

    return 0;
}
