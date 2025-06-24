# Аналіз варіантів реалізації API/UI для ModESP

## Поточний стан системи

### Існуюча архітектура
- **EventBus**: Асинхронна комунікація між модулями з пріоритетами
- **SharedState**: Централізоване сховище даних з thread-safe доступом
- **Патерн "Контракт API"**: Уникнення "магічних рядків" через system_contract.h
- **RPC Interface**: Базова підтримка через IJsonRpcRegistrar (ще не реалізована повністю)

### Конфігурація UI (ui.json)
```json
{
    "web": {
        "enabled": true,
        "port": 80
    },
    "api": {
        "enabled": true,
        "json_rpc": true,
        "rest": true
    }
}
```

## Варіанти реалізації

### 1. Централізований API Gateway Module

**Архітектура:**
```
┌─────────────────────────────────┐
│       API Gateway Module         │
├─────────┬──────────┬────────────┤
│  REST   │ JSON-RPC │ WebSocket  │
│ Handler │ Handler  │  Handler   │
└────┬────┴────┬─────┴─────┬──────┘
     │         │           │
     └─────────┴───────────┘
                │
     ┌──────────┴──────────┐
     │   Routing Layer     │
     └──────────┬──────────┘
                │
     ┌──────────┴──────────┐
     │ Command Processor   │
     └──────────┬──────────┘
                │
    ┌───────────┼───────────┐
    ▼           ▼           ▼
EventBus   SharedState   Modules
```

**Переваги:**
- Єдина точка входу для всіх API запитів
- Централізована валідація та авторизація
- Легше логування та моніторинг
- Простіше версіонування API

**Недоліки:**
- Додаткові накладні витрати на маршрутизацію
- Потенційне вузьке місце продуктивності
- Більше коду для підтримки
- Суперечить децентралізованій філософії системи

### 2. Децентралізований підхід (рекомендований)

**Архітектура:**
```
┌──────────────────┐  ┌──────────────────┐
│   WebUIModule    │  │   ApiModule      │
├──────────────────┤  ├──────────────────┤
│ - HTTP Server    │  │ - REST endpoints │
│ - WebSocket      │  │ - JSON-RPC       │
│ - Static files   │  │ - Validation     │
└────────┬─────────┘  └────────┬─────────┘
         │                     │
         └──────────┬──────────┘
                    │
        ┌───────────┴───────────┐
        │   system_contract.h   │
        └───────────┬───────────┘
                    │
         ┌──────────┴──────────┐
         ▼                     ▼
    EventBus              SharedState
         ▲                     ▲
         └──────────┬──────────┘
                    │
    ┌───────────────┼───────────────┐
    ▼               ▼               ▼
SensorModule   ActuatorModule   Other Modules
```
**Переваги:**
- Відповідає існуючій модульній архітектурі
- Мінімальні накладні витрати
- Кожен модуль відповідає за свій API
- Використовує існуючі EventBus/SharedState

**Недоліки:**
- Потребує чіткого дотримання контрактів
- Складніше відстежувати всі endpoints

### 3. Гібридний підхід

**Архітектура:**
```
┌─────────────────────────────────┐
│         WebUIModule              │
├─────────────────────────────────┤
│ - HTTP Server                   │
│ - WebSocket Manager             │
│ - Static Resource Handler       │
└────────────┬────────────────────┘
             │
    ┌────────┴────────┐
    │  API Dispatcher │
    └────────┬────────┘
             │
    ┌────────┴───────────────┐
    ▼                         ▼
Direct Module RPC        EventBus Commands
    │                         │
    ▼                         ▼
Modules with             Modules without
RPC interface            RPC interface
```

## Рекомендації

### Оптимальне рішення: Децентралізований підхід з API Dispatcher

1. **WebUIModule** - відповідає за HTTP/WebSocket сервер
2. **API Dispatcher** - тонкий шар маршрутизації без бізнес-логіки
3. **Модулі** - реєструють свої RPC методи через IJsonRpcRegistrar

### План реалізації

#### Фаза 1: Базова інфраструктура
```cpp
// system_contract.h
namespace ModespContract {
    namespace Event {
        // API події
        constexpr auto ApiRequest = "api.request";
        constexpr auto ApiResponse = "api.response";
        constexpr auto ApiError = "api.error";
    }
    
    namespace State {
        // API стан
        constexpr auto ApiStatus = "api.status";
        constexpr auto ApiMetrics = "api.metrics";
    }
}
```
#### Фаза 2: WebUIModule
```cpp
class WebUIModule : public BaseModule {
private:
    httpd_handle_t server;
    WebSocketManager ws_manager;
    ApiDispatcher api_dispatcher;
    
public:
    esp_err_t init() override;
    void configure(const nlohmann::json& config) override;
    void register_rpc(IJsonRpcRegistrar& rpc) override;
    
private:
    // HTTP handlers
    esp_err_t handle_rest_request(httpd_req_t* req);
    esp_err_t handle_websocket_request(httpd_req_t* req);
    
    // API routing
    esp_err_t route_api_request(const std::string& path, 
                               const nlohmann::json& params,
                               nlohmann::json& response);
};
```

#### Фаза 3: API Dispatcher
```cpp
class ApiDispatcher {
private:
    std::map<std::string, RpcHandler> rpc_methods;
    
public:
    // Реєстрація методів від модулів
    void register_method(const std::string& name, RpcHandler handler);
    
    // Виконання RPC
    esp_err_t execute(const std::string& method,
                     const nlohmann::json& params,
                     nlohmann::json& result);
    
    // REST to RPC mapping
    esp_err_t map_rest_to_rpc(const std::string& path,
                             const std::string& method,
                             const nlohmann::json& body,
                             nlohmann::json& result);
};
```

### Переваги рекомендованого підходу

1. **Масштабованість**: Легко додавати нові API endpoints
2. **Модульність**: Кожен модуль керує своїм API
3. **Продуктивність**: Мінімальні накладні витрати
4. **Гнучкість**: Підтримка REST, JSON-RPC, WebSocket
5. **Безпека**: Централізована точка для валідації

### Приклад використання

```cpp
// В SensorModule
void SensorModule::register_rpc(IJsonRpcRegistrar& rpc) {
    rpc.register_method("sensor.get_all", 
        [this](const json& params, json& result) {
            return get_all_sensors(result);
        });
        
    rpc.register_method("sensor.get_value",
        [this](const json& params, json& result) {
            if (!params.contains("id")) {
                return ESP_ERR_INVALID_ARG;
            }
            return get_sensor_value(params["id"], result);
        });
}
```

## Висновок

Рекомендую використовувати **децентралізований підхід з тонким API Dispatcher**. Це дозволить:
- Зберегти модульність системи
- Уникнути "товстого" API Gateway
- Використати існуючі механізми (EventBus, SharedState)
- Легко розширювати функціональність

Наступні кроки:
1. Створити system_contract.h з API контрактами
2. Реалізувати IJsonRpcRegistrar інтерфейс
3. Створити WebUIModule з HTTP сервером
4. Додати RPC методи до існуючих модулів