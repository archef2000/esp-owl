#include <stdio.h>
#include <string.h>
#include "esp_log_level.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"
#include "esp_log.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "owl/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "CERT_GEN"

#define NVS_NAMESPACE "airdrop"
#define KEY_STORAGE "private_key"
#define CERT_STORAGE "certificate"

#include <string.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_http_client.h"

// Function to retrieve stored certificate and key
static esp_err_t retrieve_cert_and_key(char **cert, char **key)
{
    esp_err_t err;

    // Initialize NVS
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS handle
    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Retrieve the private key
    size_t key_size;
    err = nvs_get_blob(nvs_handle, KEY_STORAGE, NULL, &key_size);
    if (err == ESP_OK) {
        *key = malloc(key_size);
        if (*key == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for key");
            nvs_close(nvs_handle);
            return ESP_ERR_NO_MEM;
        }
        err = nvs_get_blob(nvs_handle, KEY_STORAGE, *key, &key_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to retrieve private key: %s", esp_err_to_name(err));
            free(*key);
            *key = NULL;
            nvs_close(nvs_handle);
            return err;
        }
    } else {
        ESP_LOGE(TAG, "Private key not found: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Retrieve the certificate
    size_t cert_size;
    err = nvs_get_blob(nvs_handle, CERT_STORAGE, NULL, &cert_size);
    if (err == ESP_OK) {
        *cert = malloc(cert_size);
        if (*cert == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for certificate");
            free(*key);
            *key = NULL;
            nvs_close(nvs_handle);
            return ESP_ERR_NO_MEM;
        }
        err = nvs_get_blob(nvs_handle, CERT_STORAGE, *cert, &cert_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to retrieve certificate: %s", esp_err_to_name(err));
            free(*key);
            free(*cert);
            *key = NULL;
            *cert = NULL;
            nvs_close(nvs_handle);
            return err;
        }
    } else {
        ESP_LOGE(TAG, "Certificate not found: %s", esp_err_to_name(err));
        free(*key);
        *key = NULL;
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

// Perform HTTPS request
void https_request(char* host)
{
    char *cert = NULL;
    char *key = NULL;
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    if (retrieve_cert_and_key(&cert, &key) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to retrieve cert and key from NVS");
        return;
    }

    esp_http_client_config_t config = {
        .host = host,
        .port = 8770,
        .user_agent = "AirDrop/1.0",
        .method = HTTP_METHOD_POST,
        .path = "/Discover",
        .cert_pem = cert,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .client_key_pem = key,
        .client_cert_pem = cert,
        .skip_cert_common_name_check = true,
        .buffer_size = 2048,
        .keep_alive_enable = true,
        .if_name = (struct ifreq *)"ow1",
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_header(client, "Connection", "keep-alive");
    esp_http_client_set_header(client, "Accept", "*/*");
    esp_http_client_set_header(client, "Accept-Language", "en-us");
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d",
                 esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }
    // Get the content length
    int content_length = esp_http_client_get_content_length(client);
    ESP_LOGI(TAG, "Content Length = %d", content_length);
    
    // Read the response content
    char *buffer = malloc(content_length + 1); // +1 for null-terminator
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
    } else {
        int read_len = esp_http_client_read(client, buffer, content_length);
        if (read_len >= 0) {
            buffer[read_len] = '\0'; // Null-terminate the buffer
            ESP_LOGI(TAG, "Response Content:\n%s", buffer);
        } else {
            ESP_LOGE(TAG, "Failed to read response content");
        }
        free(buffer);
    }

    esp_http_client_cleanup(client);
    free(cert);
    free(key);
}

void _generate_and_store_cert_key(void *pvParameters)
{
    log_info("Generating private key and certificate...");
    int ret;
    mbedtls_pk_context key;
    mbedtls_x509_crt cert;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "gen_cert";

    // Initialize contexts
    mbedtls_pk_init(&key);
    mbedtls_x509_crt_init(&cert);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    // Seed the random number generator
    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers, strlen(pers))) != 0) {
        ESP_LOGE(TAG, "Failed to seed RNG: %d", ret);
        goto cleanup;
    }

    // Generate RSA key pair
    if ((ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA))) != 0) {
        ESP_LOGE(TAG, "Failed to setup PK context: %d", ret);
        goto cleanup;
    }

    if ((ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &ctr_drbg, 2048, 65537)) != 0) {
        ESP_LOGE(TAG, "Failed to generate RSA key: %d", ret);
        goto cleanup;
    }

    // Create self-signed certificate
    mbedtls_x509write_cert crt;
    mbedtls_x509write_crt_init(&crt);
    mbedtls_x509write_crt_set_subject_name(&crt, "CN=test");
    mbedtls_x509write_crt_set_issuer_name(&crt, "CN=test");
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    unsigned char serial[16]; // 16-byte serial number
    ret = mbedtls_ctr_drbg_random(&ctr_drbg, serial, sizeof(serial));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to generate serial number: %d", ret);
        goto cleanup;
    }
    mbedtls_x509write_crt_set_serial_raw(&crt, serial,sizeof(serial));
    mbedtls_x509write_crt_set_validity(&crt, "20250101000000", "20260101000000");
    mbedtls_x509write_crt_set_subject_key(&crt, &key);
    mbedtls_x509write_crt_set_issuer_key(&crt, &key);

    unsigned char cert_buf[4096];
    ret = mbedtls_x509write_crt_pem(&crt, cert_buf, sizeof(cert_buf), mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to write certificate: %d", ret);
        goto cleanup;
    }

    unsigned char key_buf[2048];
    ret = mbedtls_pk_write_key_pem(&key, key_buf, sizeof(key_buf));
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to write private key: %d", ret);
        goto cleanup;
    }

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS handle
    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        goto cleanup;
    }

    // Store private key
    err = nvs_set_blob(nvs_handle, KEY_STORAGE, key_buf, strlen((char *)key_buf) + 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store private key: %s", esp_err_to_name(err));
    }

    // Store certificate
    err = nvs_set_blob(nvs_handle, CERT_STORAGE, cert_buf, strlen((char *)cert_buf) + 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store certificate: %s", esp_err_to_name(err));
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit changes: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    ESP_LOGE(TAG, "Private key and certificate stored successfully!");

cleanup:
    log_error("Cleaning up...");
    mbedtls_pk_free(&key);
    mbedtls_x509_crt_free(&cert);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    xTaskNotifyGive((TaskHandle_t)pvParameters);
    vTaskDelete(NULL);
}

void generate_and_store_cert_key()
{
    TaskHandle_t main_task = xTaskGetCurrentTaskHandle();
    xTaskCreate(_generate_and_store_cert_key, "generate_and_store_cert_key", 2048*8, main_task, 5, NULL);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

void _print_stored_cert_and_key(void *pvParameters)
{
    esp_err_t err;

    // Initialize NVS
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS handle
    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }

    // Retrieve the private key
    size_t key_size;
    err = nvs_get_blob(nvs_handle, KEY_STORAGE, NULL, &key_size);
    if (err == ESP_OK) {
        char *key_buf = malloc(key_size);
        if (key_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for key");
        } else {
            err = nvs_get_blob(nvs_handle, KEY_STORAGE, key_buf, &key_size);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Private Key:\n%s", key_buf);
            } else {
                ESP_LOGE(TAG, "Failed to retrieve private key: %s", esp_err_to_name(err));
            }
            free(key_buf);
        }
    } else {
        ESP_LOGE(TAG, "Private key not found: %s", esp_err_to_name(err));
    }

    // Retrieve the certificate
    size_t cert_size;
    err = nvs_get_blob(nvs_handle, CERT_STORAGE, NULL, &cert_size);
    if (err == ESP_OK) {
        char *cert_buf = malloc(cert_size);
        if (cert_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for certificate");
        } else {
            err = nvs_get_blob(nvs_handle, CERT_STORAGE, cert_buf, &cert_size);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Certificate:\n%s", cert_buf);
            } else {
                ESP_LOGE(TAG, "Failed to retrieve certificate: %s", esp_err_to_name(err));
            }
            free(cert_buf);
        }
    } else {
        ESP_LOGE(TAG, "Certificate not found: %s", esp_err_to_name(err));
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    xTaskNotifyGive((TaskHandle_t)pvParameters);
    vTaskDelete(NULL);
}

void print_stored_cert_and_key()
{
    TaskHandle_t main_task = xTaskGetCurrentTaskHandle();
    xTaskCreate(_print_stored_cert_and_key, "print_stored_cert_and_key", 2048*8, main_task, 5, NULL);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

void test_cert()
{
    generate_and_store_cert_key();
    print_stored_cert_and_key();
    vTaskDelete(NULL);
}