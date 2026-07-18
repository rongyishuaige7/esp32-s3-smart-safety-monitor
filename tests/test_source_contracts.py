from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


class SourceContracts(unittest.TestCase):
    def test_platform_target_and_generated_page_contract(self):
        ini = read("platformio.ini")
        cmake = read("main/CMakeLists.txt")
        self.assertIn("platform = espressif32@6.13.0", ini)
        self.assertIn("board = esp32-s3-devkitc-1", ini)
        self.assertIn("framework = espidf", ini)
        pre = read("pio_pre_embed_web.py")
        self.assertIn(".pio/generated/web_index_embed.c", cmake)
        self.assertIn('project / "tools" / "embed_web.py"', pre)
        self.assertFalse((ROOT / "main/web_index_embed.c").exists())

    def test_public_network_defaults_are_empty_and_local_file_is_ignored(self):
        example = read("main/local_config.example.h")
        ignored = read(".gitignore")
        server = read("main/web_server.c")
        self.assertIn('#define SAFETY_MONITOR_AP_SSID ""', example)
        self.assertIn('#define SAFETY_MONITOR_AP_PASSWORD ""', example)
        self.assertIn("main/local_config.h", ignored)
        self.assertFalse((ROOT / "main/local_config.h").exists())
        self.assertIn('local_config.example.h', server)
        self.assertIn("web_server_has_local_network_config", server)
        self.assertNotIn('"SafetyMonitor"', server)
        self.assertNotIn('"12345678"', server)
        self.assertNotIn("PASS=%s", server)

    def test_source_gpio_and_threshold_contracts(self):
        config = read("main/app_config.h")
        self.assertIn("#define PIN_MQ2_ADC_GPIO        1", config)
        self.assertIn("#define PIN_MQ5_ADC_GPIO        2", config)
        self.assertIn("#define PIN_FLAME_GPIO          4", config)
        self.assertIn("#define PIN_HCSR04_TRIG         5", config)
        self.assertIn("#define PIN_HCSR04_ECHO         6", config)
        self.assertIn("#define THRESHOLD_SMOKE_LEVEL1  800U", config)
        self.assertIn("#define THRESHOLD_SMOKE_LEVEL2  1500U", config)
        self.assertIn("#define THRESHOLD_SMOKE_LEVEL3  2200U", config)
        self.assertIn("#define THRESHOLD_GAS_PPM_RAW   2000U", config)

    def test_page_is_not_a_fake_runtime_demo_and_labels_match_source(self):
        page = read("main/web/index.html")
        for value in ("L1 &ge;800", "L2 &ge;1500", "L3 &ge;2200", "RAW &ge;2000", "静态预览：未连接设备"):
            self.assertIn(value, page)
        self.assertNotIn("startDemo", page)
        self.assertNotIn("Math.random", page)
        self.assertNotIn("192.168.4.1", page)
        self.assertNotIn("fonts.googleapis.com", page)
        self.assertIn("WAITING &middot; 等待数据", page)
        self.assertIn("不是消防告警结论", page)

    def test_docs_keep_safety_boundaries_visible(self):
        readme = read("README.md")
        hardware = read("HARDWARE.md")
        security = read("SECURITY.md")
        verification = read("docs/VERIFICATION.md")
        self.assertIn("不是火灾报警器", readme)
        self.assertIn("不是 PCB 原理图", hardware)
        self.assertIn("无认证、无 TLS", security)
        self.assertIn("# 构建说明", verification)


if __name__ == "__main__":
    unittest.main()
