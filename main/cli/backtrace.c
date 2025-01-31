#include "esp_log.h"

#define TAG "BacktraceExample"

void func10(); // Declare all functions upfront
void func9() { ESP_LOGI(TAG, "In func9"); func10(); }
void func8() { ESP_LOGI(TAG, "In func8"); func9(); }
void func7() { ESP_LOGI(TAG, "In func7"); func8(); }
void func6() { ESP_LOGI(TAG, "In func6"); func7(); }
void func5() { ESP_LOGI(TAG, "In func5"); func6(); }
void func4() { ESP_LOGI(TAG, "In func4"); func5(); }
void func3() { ESP_LOGI(TAG, "In func3"); func4(); }
void func2() { ESP_LOGI(TAG, "In func2"); func3(); }
void func1() { ESP_LOGI(TAG, "In func1"); func2(); }
void func10() { ESP_LOGI(TAG, "In func10"); *(int *)0 = 0; } // Intentional crash
 
void test_backtrace(void) {
    ESP_LOGI(TAG, "Starting backtrace example");
    func1();
    ESP_LOGI(TAG, "This should not print!");
}
