//
// Includes
//

// stdlib
#include <stdio.h>
#include <string.h>

// uni.net
#include <uni_net.h>

// ox
#include "server_http/ox_server_http_dashboard.h"

// ox.assets
#include "ox_asset_bootstrap.h"
#include "ox_asset_bootstrap_js.h"

#include "ox_asset_fontawesome_css.h"
#include "ox_asset_fontawesome_wf.h"

#include "ox_asset_index.h"
#include "ox_asset_dashboard.h"
#include "ox_asset_favicon.h"
#include "io/uni_hal_io_stdio.h"


//
// Private
//
static size_t _ox_server_dashboard_handler_status(void* userdata, uint8_t* buf_out, size_t buf_out_size, const uint8_t* buf_in, size_t buf_in_len) {
    (void)buf_in;
    (void)buf_in_len;
    (void)userdata;

    if (!buf_out || !buf_out_size) {
        return 0;
    }

    return uni_hal_io_stdio_snprintf((char*)buf_out, buf_out_size, "{}");
}

//
// Implementation
//
bool ox_server_dashboard_init(ox_server_dashboard_context_t* ctx, uni_net_http_server_context_t* server) {
    (void)ctx;
    bool result = false;
    if (server != NULL) {
        result = true;
        result &= uni_net_http_server_register_handler_ex(server, UNI_NET_HTTP_COMMAND_GET, "/status.json", _ox_server_dashboard_handler_status, ctx);
        result &= uni_net_http_server_register_file_ex(server, "/", g_ox_asset_index, g_ox_asset_index_size);
        result &= uni_net_http_server_register_file_ex(server, "/dashboard.html", g_ox_asset_dashboard, g_ox_asset_dashboard_size);
        result &= uni_net_http_server_register_file_ex(server, "/favicon.ico", g_ox_asset_favicon, g_ox_asset_favicon_size);
        result &= uni_net_http_server_register_file_ex(server, "/bootstrap.min.css", g_ox_asset_bootstrap, g_ox_asset_bootstrap_size);
        result &= uni_net_http_server_register_file_ex(server, "/bootstrap.bundle.min.js", g_ox_asset_bootstrap_js, g_ox_asset_bootstrap_js_size);
        result &= uni_net_http_server_register_file_ex(server, "/css/fontawesome.min.css", g_ox_asset_fontawesome_css, g_ox_asset_fontawesome_css_size);
        result &= uni_net_http_server_register_file_ex(server, "/webfonts/fa-solid-900.woff2", g_ox_asset_fontawesome_wf, g_ox_asset_fontawesome_wf_size);

    }
    return result;
}
