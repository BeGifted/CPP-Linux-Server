#include "../chat/chat.h"
#include <assert.h>

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void test_assert() {
    CHAT_LOG_INFO(g_logger) << chat::BacktraceToString(10);
    CHAT_ASSERT2(0 == 1, "abcdef xx");
}

int main(int argc, char** argv) {
    test_assert();

    // int arr[] = {1,3,5,7,9,11};

    // CHAT_LOG_INFO(g_logger) << chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 0);
    // CHAT_LOG_INFO(g_logger) << chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 1);
    // CHAT_LOG_INFO(g_logger) << chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 4);
    // CHAT_LOG_INFO(g_logger) << chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 13);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 0) == -1);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 1) == 0);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 2) == -2);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 3) == 1);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 4) == -3);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 5) == 2);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 6) == -4);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 7) == 3);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 8) == -5);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 9) == 4);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 10) == -6);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 11) == 5);
    // CHAT_ASSERT(chat::BinarySearch(arr, sizeof(arr) / sizeof(arr[0]), 12) == -7);
    return 0;
}