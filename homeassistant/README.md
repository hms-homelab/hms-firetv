# Home Assistant side of the Fire TV remote

Two files, neither loaded from this repository — they are kept here so the
Home Assistant half is version controlled with the service it drives.

| File | What it is |
|---|---|
| `firetv-dashboard.yaml` | The Lovelace config for `/firetv-control`. Source of truth — edit this. |
| `ha-config.yaml` | The `rest_command`, `rest` sensor, `input_text` and `script` entries. A valid HA package. |

## Pushing the dashboard

Over the authenticated websocket, not by editing `.storage/lovelace.*`: Home
Assistant holds lovelace state in memory and rewrites those files itself, so an
edit made underneath a running instance gets clobbered. The websocket lands it
live with no restart.

```bash
export HA_URL=http://192.168.2.7:8123
export HA_BEARER_TOKEN=...
python3 push_dashboard.py firetv-dashboard.yaml     # url_path: firetv-control
```

Each push saves the previous config beside the yaml as `.previous.json` for
rollback. That file is deliberately not tracked.

## The rest of it

`ha-config.yaml` currently lives **inline** in the instance's
`configuration.yaml` and `scripts.yaml`. Do not also add it as a package there
or every key ends up defined twice. It is in package form so a rebuilt
instance can take it directly.

After changing it on the live instance:
`rest_command.reload`, `script.reload`, `input_text.reload`, `rest.reload`.

## Requirements

HACS cards: `button-card`, `config-template-card`, `card-mod`,
`lovelace-mushroom`. The app grid needs `config-template-card`; the dark
styling of the stock cards needs `card-mod`, and card-mod only patches cards on
a genuine page load, so hard-refresh after installing it.

## Gotchas worth keeping

- `config-template-card` evaluates `eval(template.substring(2, length - 1))` —
  it strips `${` and **exactly one** trailing character. A `>` folded scalar
  leaves a trailing newline, so it eats that instead of the closing brace and
  the grid silently renders nothing. Use `>-`, and keep every line of the
  template at the same indentation: a deeper-indented line in a folded scalar
  keeps a real newline, and the `${...}` match does not span newlines.
- `button-card` `styles` must be a list of single-key **maps**. A list of lists
  gives `Failed to set an indexed property [0] on 'CSSStyleDeclaration'`.
- A view `background:` with `fixed` is sized to the viewport, so everything
  below the fold falls back to the light ground. Leave `fixed` off.
- `horizontal-stack` sizes fixed-width children to content and left-aligns
  them. Centre with card-mod `#root { justify-content: center; }` rather than
  padding the row with invisible spacer cards.
