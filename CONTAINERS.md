# Project containers

Naturalehia has one persistent development container for each active project.
Container order follows the project order in the repository README, and every
container name is `naturalehia-` followed by its project directory name.

| Order | Project | Persistent container | Host launcher |
| ---: | --- | --- | --- |
| 1 | The Elder Brother of Fauna | `naturalehia-the-elder-brother-of-fauna` | `projects/the-elder-brother-of-fauna/container.sh` |
| 2 | Logic Gates of the Biological Kingdom | `naturalehia-logic-gates-of-the-biological-kingdom` | `projects/logic-gates-of-the-biological-kingdom/container.sh` |
| 3 | Fostering Cellular Agriculture | `naturalehia-fostering-cellular-agriculture` | `projects/fostering-cellular-agriculture/container.sh` |

`projects/incorporating-plastic-into-the-lifecycle` is an unpublished,
untested research note rather than an active project. It has no executable
environment, so it does not have a container definition.

## Runtime matrix

| Project | Digest-pinned base image | Bind mount | Published ports | Devices | Persistent command |
| --- | --- | --- | --- | --- | --- |
| The Elder Brother of Fauna | `debian:13-slim@sha256:020c0d20b9880058cbe785a9db107156c3c75c2ac944a6aa7ab59f2add76a7bd` | Repository checkout at `/workspace/naturalehia` | `127.0.0.1:8080:8080`, `127.0.0.1:50051:50051` | All NVIDIA GPUs, `compute,utility` | `sleep infinity` |
| Logic Gates of the Biological Kingdom | `debian:13-slim@sha256:020c0d20b9880058cbe785a9db107156c3c75c2ac944a6aa7ab59f2add76a7bd` | Project checkout at `/workspace/logic-gates-of-the-biological-kingdom` | `127.0.0.1:38417:38417` | All NVIDIA GPUs by default; explicit `none` mode supported | Bash wait loop |
| Fostering Cellular Agriculture | `emscripten/emsdk:6.0.5@sha256:76a44fff907397784decc435115d07fcb9587a4f1504977f39f3745e538e3a1e` | Project checkout at `/workspace/fostering-cellular-agriculture` | None | None | Bash wait loop |

All three definitions use restart policy `no`, an init process, a non-root
development identity, local logging with rotation, and a host-side
configuration fingerprint. The launchers resolve and record the checkout's
actual host path before creating anything.

## Responsibility split

Each active project uses the same three-part interface:

- `setup.sh` runs inside the container and only installs reproducible
  dependencies and configures the environment. It has no command dispatcher
  and never starts, stops, inspects, recreates, builds, tests, or runs the
  project.
- `container.sh` runs on the host and owns container lifecycle and access:
  `up`, `shell`, `exec`, `status`, `stop`, and the explicitly destructive
  `recreate` operation.
- The project's Makefile, CMake presets, or other native task runner owns
  build, test, and run operations.

For example:

```bash
cd projects/logic-gates-of-the-biological-kingdom
bash container.sh up
bash container.sh exec make test
```

## Persisted data and legacy names

The canonical container names are new. Existing Fauna and Logic Gates volume
names are retained where they contain project data; the new Cellular
Agriculture definition receives its own project-specific home volume:

| Project | Persistent named-volume mounts |
| --- | --- |
| The Elder Brother of Fauna | `naturalehia-build:/work/naturalehia-build`, `naturalehia-home:/home/developer`, `naturalehia-gpu:/opt/naturalehia-gpu` |
| Logic Gates of the Biological Kingdom | `naturalehia-protein-logic-build-v1:/work`, `naturalehia-protein-logic-home-v1:/home/developer` |
| Fostering Cellular Agriculture | `naturalehia-fostering-cellular-agriculture-home-v1:/home/developer` |

The launchers refuse to create a canonical container when a recognized legacy
container exists, and they refuse to adopt an unmanaged or configuration-
mismatched canonical container. They never remove or reinitialize a named
volume during normal launch. The unversioned
`naturalehia-protein-logic-build` and `naturalehia-protein-logic-home` volumes
are protected legacy data and are not mounted by the current definition.

Docker cannot rename a volume in place. Normalizing retained volume names
therefore requires a separately approved copy-and-verify migration with the
original volumes kept for rollback; it is not part of container recovery.
