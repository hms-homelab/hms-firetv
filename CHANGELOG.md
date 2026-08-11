# Changelog

## [1.1.1] - 2026-08-11

### Fixed
- Live microphone sent nothing after the first few presses. The capture graph
  kept only the ScriptProcessor in a variable; the source and gain nodes were
  locals, and once they were collected the audio callback stopped firing while
  the socket stayed open and healthy, so a session opened and closed with zero
  bytes. Capture now uses an AudioWorklet and every node is retained.
- Audio spoken before the session was ready was dropped. Opening a session
  wakes the device and can take twelve seconds; the button now buffers from the
  moment it is pressed and flushes on ready, and releasing early still sends
  what was captured.
- The page reports how much audio it has sent, so a silent path is visible
  rather than having to be inferred from the service log.

### Added
- `__voiceSelfTest()` on the voice page, which pushes a tone through the same
  send path the microphone uses - the relay can be proven from a machine with
  no microphone.

## [1.1.0] - 2026-08-11

### Added
- Alexa voice control. The device voice channel carries raw PCM at 16 kHz mono
  over a WebSocket on port 9090, never text, so text is synthesised with a
  Wyoming TTS engine (`TTS_HOST`) and streamed to the device microphone.
  Three modes: session bookends, speak text or a file/URL, and a live relay.
- Live microphone relay at `/ws/voice`, with a push-to-talk page at
  `static/voice.html`. Needs TLS, so an optional listener was added behind
  `API_SSL_PORT` — browsers only grant a microphone to a secure context.
- Press and hold. Navigation can now send a keyDown/keyUp pair, which is what
  makes the device repeat a key; `POST /api/devices/:id/hold` and `/key`.
- Installed app list from the device itself via `appsV2`, stored by
  `AppSyncService` and exposed as `POST /api/devices/:id/apps/sync`. Home
  Assistant gets a select entity holding the real apps.
- Home Assistant dashboard rebuilt over the REST API, in `homeassistant/`.

### Changed
- A sleeping device is now woken and the command retried once, in both the
  synchronous client and the async one. Only a refused connection triggers it;
  retrying a timeout only doubles the wait for a device that is absent.
- Requests use a 2 second connect timeout, so an unreachable device fails fast
  instead of burning the full command timeout.

### Removed
- ADB. Its only purpose was listing installed apps, which `appsV2` now does
  without debugging mode or port 5555. `Device::adb_enabled` and the
  `discovered_via_adb` column are gone.

### Fixed
- `device_apps` was missing `sort_order` while `getAppsForDevice` selected it,
  so that query had been failing on PostgreSQL and no app list ever reached
  Home Assistant.
- Four test suites initialised the database service but never wired it into the
  repositories, so every repository call short-circuited and the controllers
  answered 500 — and the fallback pointed at the production database. Each now
  builds its own in-memory SQLite. Suite goes 6/10 to 10/10.
- `SELECT *` on `fire_tv_devices` replaced with explicit column lists; the
  SQLite parser reads by position and would have misaligned every field.

## [1.0.7] - 2026-08-09

### Fixed
- **Connection pool leaked a slot on every failed reconnect**: when `acquire()` popped a dead connection and could not replace it, it threw without returning the slot to the pool. A database outage therefore drained the pool permanently — once all 8 slots were lost the queue stayed empty forever and every subsequent query blocked for the full 5s timeout and reported `Connection pool timeout: no connection available within 5000ms`, **even after PostgreSQL came back**. The service reported `status: degraded` / `database: disconnected` from 2026-08-06 23:42 until manually restarted (4602 failed queries). The pool now tracks how many connections it owns, frees the slot on every failure path, and refills on demand — so it recovers by itself once the database returns, and a pool built while the database is down is no longer permanently dead.
- **Connectivity errors were reported as pool timeouts**: an unreachable database produced a misleading `Connection pool timeout` after a 5s wait instead of the actual connection error. Real errors now surface immediately.

### Added
- `tests/test_connection_pool.cpp` — regression coverage for the leak (fails against the old pool), plus RAII return, capacity stability across 50 acquire/release cycles, and on-demand refill. Live-database cases skip cleanly when no PostgreSQL is reachable; override the DSN with `TEST_PG_DSN`.
- `ConnectionPool::liveCount()` for pool diagnostics.

## [1.0.5] - 2026-05-03

### Added
- **Discovery UI**: Devices page now has a scan button that discovers Fire TVs on the network and lets you add them directly from results

### Fixed
- **Pairing flow**: fixed bug in pairing process that prevented successful device pairing
- **Service template**: sanitized `hms-firetv.service` — hardcoded credentials replaced with `CHANGE_ME` placeholders, username replaced with `%i` specifier, path corrected to `projects/hms-firetv/build/`

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
