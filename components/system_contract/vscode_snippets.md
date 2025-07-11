# VSCode Snippets for ModESP

Add these snippets to `.vscode/modesp.code-snippets` for faster development:

```json
{
    "Subscribe to Event": {
        "prefix": "modesp_subscribe",
        "body": [
            "EventBus::subscribe(Events::${1|SENSOR_READING_UPDATED,SENSOR_ERROR,SENSOR_CALIBRATION_COMPLETE|},",
            "    [this](const EventBus::Event& event) {",
            "        ${2:// Handle event}",
            "    });"
        ],
        "description": "Subscribe to ModESP event"
    },
    
    "Publish Event": {
        "prefix": "modesp_publish",
        "body": [
            "EventBus::publish(Events::${1|SENSOR_ERROR,SENSOR_READING_UPDATED|}, {",
            "    ${2:// Event data}",
            "});"
        ],
        "description": "Publish ModESP event"
    },
    
    "Get SharedState Value": {
        "prefix": "modesp_get_state",
        "body": [
            "${1|float,bool,int,auto|} ${2:value} = SharedState::get<$1>(States::${3:STATE_KEY});"
        ],
        "description": "Get value from SharedState"
    },
    
    "Set SharedState Value": {
        "prefix": "modesp_set_state",
        "body": [
            "SharedState::set(States::${1:STATE_KEY}, ${2:value});"
        ],
        "description": "Set value in SharedState"
    }
}
```

## Usage

1. Type `modesp_` in VSCode to see all available snippets
2. Select the snippet you need
3. Fill in the placeholders
4. Tab through to complete

## Contributing

When adding new events/states to the system:
1. Update your module manifest
2. Run `python tools/process_manifests.py`
3. Update these snippets if needed
