# Changelog

## [1.0.4] - 2026-04-28

### Fixed
- **Web UI controls broken**: Frontend called `/api/devices/{id}/navigation` and `/keyboard` but backend routes were `/navigate` and `/text` — commands silently returned SPA HTML instead of JSON
- **SPA fallback masking API 404s**: Drogon custom error handler now returns JSON 404 for `/api/*`, `/health`, `/status` paths instead of serving `index.html`

## [1.0.3] - 2026-04-17

### Added
- Angular 21 web UI replacing vanilla JS frontend (dashboard, remote, devices, apps, settings)
- DiscoveryService: automatic Fire TV IP detection via port 8009 probing + token-based matching
- Favorite apps quick-launch on Remote Control page
- SPA fallback in Drogon for Angular routing
- Enriched `/status` endpoint with DB/MQTT connection state and device counts
- `build_and_deploy.sh` script with env variable support
- GitHub Actions release workflow for binary artifacts

### Changed
- Dockerfile upgraded to 3-stage build (Angular + C++ + runtime)
- Static file cache enabled (1 hour TTL)

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
