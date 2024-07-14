// Description:
// This is the main entry point for the esp-idf-wav-player project.
// It will setup the sdcard and i2s and play a wav file.
//
// The wav file must be 16bit, 44.1kHz, stereo and have a 44 byte header.
#include "esp_system.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "driver/i2s_std.h" // i2s setup

#include <dirent.h>
#include "sdcard.h"			// sdcard intialization
#include "configuration.h"	// basic sysetm includes and pin setup

#define REBOOT_WAIT 	3000	// reboot after 5 seconds
#define AUDIO_BUFFER 	2048			// buffer size for reading the wav file and sending to i2s
// #define WAV_FILE "/sdcard/test.wav" // wav file to play
// #define WAV_FILE "/littlefs/music.wav" // wav file to play
#define WAV_FILE "/littlefs/piano2-CoolEdit.mp3_mono.wav" // wav file to play

const char *TAG = "wav-player";
i2s_chan_handle_t tx_handle;

esp_err_t i2s_setup(void)
{
	// setup a standard config and the channel
	i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
	ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

	// setup the i2s config
	i2s_std_config_t std_cfg = {
		// .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(24000), // the wav file sample rate
		.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000), // the wav file sample rate
		// .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),													 // the wav file sample rate
		// .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO), // the wav faile bit and channel config
		.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), // the wav faile bit and channel config
		.gpio_cfg = {
			// refer to configuration.h for pin setup
			.mclk = I2S_SCLK_PIN,
			.bclk = I2S_BLK_PIN,
			.ws = I2S_WS_PIN,
			.dout = I2S_DATA_OUT_PIN,
			.din = I2S_DATA_IN_PIN,
			.invert_flags = {
				.mclk_inv = false,
				.bclk_inv = false,
				.ws_inv = false,
			},
		},
	};
	return i2s_channel_init_std_mode(tx_handle, &std_cfg);
}

esp_err_t play_wav(char *fp)
{
	FILE *fh = fopen(fp, "rb");
	if (fh == NULL)
	{
		ESP_LOGE(TAG, "Failed to open file: %s", fp);
		return ESP_ERR_INVALID_ARG;
	}

	// skip the header...
	fseek(fh, 44, SEEK_SET);

	// create a writer buffer
	int16_t *buf = calloc(AUDIO_BUFFER, sizeof(int16_t));
	size_t bytes_read = 0;
	size_t bytes_written = 0;

	bytes_read = fread(buf, sizeof(int16_t), AUDIO_BUFFER, fh);

	i2s_channel_enable(tx_handle);

	while (bytes_read > 0)
	{
		// write the buffer to the i2s
		i2s_channel_write(tx_handle, buf, bytes_read * sizeof(int16_t), &bytes_written, portMAX_DELAY);
		bytes_read = fread(buf, sizeof(int16_t), AUDIO_BUFFER, fh);
		ESP_LOGV(TAG, "Bytes read: %d", bytes_read);
	}

	i2s_channel_disable(tx_handle);
	free(buf);

	return ESP_OK;
}


void app_fs_init()
{
	ESP_LOGI(TAG, "Initializing LittleFS");

	esp_vfs_littlefs_conf_t conf = {
		.base_path = "/littlefs",
		.partition_label = "storage",
		.format_if_mount_failed = true,
		.dont_mount = false,
	};

	// Use settings defined above to initialize and mount LittleFS filesystem.
	// Note: esp_vfs_littlefs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_littlefs_register(&conf);

	if (ret != ESP_OK)
	{
		if (ret == ESP_FAIL)
		{
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		}
		else if (ret == ESP_ERR_NOT_FOUND)
		{
			ESP_LOGE(TAG, "Failed to find LittleFS partition");
		}
		else
		{
			ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
		}
		return;
	}

	size_t total = 0, used = 0;
	ret = esp_littlefs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
		esp_littlefs_format(conf.partition_label);
	}
	else
	{
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	}

	// Use POSIX and C standard library functions to work with files.
	// First create a file.
	ESP_LOGI(TAG, "Opening file");
	FILE *f = fopen("/littlefs/hello.txt", "w");
	if (f == NULL)
	{
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}
	fprintf(f, "Hello World!\n");
	fclose(f);
	ESP_LOGI(TAG, "File written");
}

void listFiles(const char *path)
{
	DIR *dir;
	struct dirent *ent;

	printf("\n---------------------------\n");
	printf("Files in directory '%s':\n", path);
	dir = opendir(path);
	if (dir != NULL)
	{
		while ((ent = readdir(dir)) != NULL)
		{
			printf("\t%s/%s\n", path, ent->d_name);
		}
		closedir(dir);
	}
	else
	{
		perror("无法打开目录");
		exit(EXIT_FAILURE);
	}

	printf("\n---------------------------\n");
}

void app_main(void)
{
	ESP_LOGI(TAG, "Starting up");

	// basic info
	(void)print_system_info();

	// sdcard init
	// ESP_ERROR_CHECK(init_sdcard());
	// (void)print_sdcard_info();

	app_fs_init();
	// lfs_ls(&lfs, "/");
	listFiles("/littlefs");
	// setup i2s
	ESP_LOGI(TAG, "Setting up i2s");
	ESP_ERROR_CHECK(i2s_setup());

	// play the wav file
	ESP_LOGI(TAG, "Playing wav file");
	ESP_ERROR_CHECK(play_wav(WAV_FILE));

	// that'll do pig... that'll do
	i2s_del_channel(tx_handle); // delete the channel

	// reboot
	ESP_LOGI(TAG, "Rebooting in %d ms...", REBOOT_WAIT);
	vTaskDelay(REBOOT_WAIT / portTICK_PERIOD_MS);
	esp_restart();
}