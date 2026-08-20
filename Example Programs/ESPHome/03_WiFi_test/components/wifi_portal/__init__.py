import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import globals as globals_component, wifi
from esphome.const import CONF_ID

AUTO_LOAD = ["globals"]
DEPENDENCIES = ["esp32", "wifi"]

CONF_UI_SSID = "ui_ssid"
CONF_UI_STATUS = "ui_status"
CONF_UI_IP = "ui_ip"
CONF_UI_HINT = "ui_hint"
CONF_SELECTED_SSID = "selected_ssid"
CONF_CONNECTION_STATUS = "connection_status"
CONF_STA_IP = "sta_ip"

wifi_portal_ns = cg.esphome_ns.namespace("wifi_portal")
WifiPortalComponent = wifi_portal_ns.class_("WifiPortalComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WifiPortalComponent),
        cv.Required(CONF_UI_SSID): cv.use_id(globals_component.GlobalsComponent),
        cv.Required(CONF_UI_STATUS): cv.use_id(globals_component.GlobalsComponent),
        cv.Required(CONF_UI_IP): cv.use_id(globals_component.GlobalsComponent),
        cv.Required(CONF_UI_HINT): cv.use_id(globals_component.GlobalsComponent),
        cv.Required(CONF_SELECTED_SSID): cv.use_id(globals_component.GlobalsComponent),
        cv.Required(CONF_CONNECTION_STATUS): cv.use_id(globals_component.GlobalsComponent),
        cv.Required(CONF_STA_IP): cv.use_id(globals_component.GlobalsComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # Bind YAML globals to the C++ component. The component publishes state into
    # these globals, and the YAML LVGL script performs the actual UI update.
    # ESPHome's WiFi component also consumes IDF scan results in its
    # WIFI_EVENT_SCAN_DONE handler, so keep the full cached list for the portal.
    wifi.request_wifi_scan_results()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    bindings = (
        (CONF_UI_SSID, "set_ui_ssid"),
        (CONF_UI_STATUS, "set_ui_status"),
        (CONF_UI_IP, "set_ui_ip"),
        (CONF_UI_HINT, "set_ui_hint"),
        (CONF_SELECTED_SSID, "set_selected_ssid"),
        (CONF_CONNECTION_STATUS, "set_connection_status"),
        (CONF_STA_IP, "set_sta_ip"),
    )
    for key, setter in bindings:
        glob = await cg.get_variable(config[key])
        cg.add(getattr(var, setter)(glob))
