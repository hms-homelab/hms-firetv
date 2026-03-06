# Changelog

## [1.0.2] - 2026-03-05

### Fixed
- **MQTT reconnect zombie bug**: After connection loss, paho auto-reconnect restored TCP but
  `connected_` flag stayed `false` (no `set_connected_handler`). Service became deaf — running
  but unable to publish or receive commands. Added `onReconnected()` callback that restores
  `connected_` and re-subscribes all device topics (lost due to `clean_session=true`).
- **Pre-existing linker error**: Fixed paho-mqttpp3 CMake target mismatch that prevented main
  binary from building.

## [1.0.1] - 2026-02-25

### Added
- Multi-arch Docker image (amd64 + arm64) published to GHCR
- GitHub Actions CI/CD workflow for automated builds
- `.dockerignore` for optimized Docker context
- `VERSION` file for image tagging

### Changed
- Dockerfile rewritten: Debian trixie base with repo packages (no source builds)
- CMakeLists.txt: use PahoMqttCpp cmake target instead of hardcoded path
- Tests made optional via `BUILD_TESTS` cmake option

### Organized
- 18 documentation files moved from root to `docs/`

## [1.0.0] - 2026-02-03

### Added
- Initial release: Fire TV control via ADB + MQTT + REST API
- Background logger with LRU cache
- Connection pool for PostgreSQL
- Device auto-detection and wake support
- MQTT command handler with extensible architecture
- Drogon-based REST API on port 8888
