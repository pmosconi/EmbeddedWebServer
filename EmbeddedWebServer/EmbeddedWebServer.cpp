#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "mongoose.h"
#include "../libjson/libjson.h"

static bool silent;

static const char *html_form =
"<html><body>POST example."
"<form method=\"POST\" action=\"/handle_post_request\">"
"Input 1: <input type=\"text\" name=\"input_1\" /> <br/>"
"Input 2: <input type=\"text\" name=\"input_2\" /> <br/>"
"<input type=\"submit\" />"
"</form></body></html>";

// Console message printing if not silent mode
void inline silent_printf(bool silent, const char* format, ...) {
	if (!silent) {
		va_list ap;
		va_start(ap, format);
		vprintf(format, ap);
		va_end(ap);
	}
}

// Parse JSON from GET
void ParseJSON(struct mg_connection *conn, const JSONNode & n) {
	JSONNode::const_iterator i = n.begin();
	mg_printf_data(conn, "-- Array or Node Start\n");
	silent_printf(silent, "-- Array or Node Start\n");
	while (i != n.end()){
		// recursively call ourselves to dig deeper into the tree
		if (i->type() == JSON_ARRAY || i->type() == JSON_NODE) {
			ParseJSON(conn, *i);
		}
		// get the node name and value as a string
		std::string node_name = i->name();
		std::string node_value = i->as_string();

		mg_printf_data(conn,
			"Node Name: [%s] "
			"Node Value: [%s]\n",
			node_name.c_str(), node_value.c_str());
		silent_printf(silent,
			"Node Name: [%s] "
			"Node Value: [%s]\n",
			node_name.c_str(), node_value.c_str());

		//increment the iterator
		++i;
	}
	mg_printf_data(conn, "-- Array or Node End\n");
	silent_printf(silent, "-- Array or Node End\n");

}

// Websocket reply manager
// Send JSON containing received data
// Test with http://www.websocket.org/echo.html
static mg_result send_reply_websocket(struct mg_connection *conn) {
	JSONNode n(JSON_NODE);
	std::string responseString(conn->content, conn->content_len);
	n.push_back(JSONNode("Response", ""));
	JSONNode c(JSON_NODE);
	c.push_back(JSONNode("Response Data", responseString));
	c.push_back(JSONNode("Response Lenght", conn->content_len));
	n.push_back(c);
	std::string jc = n.write();
	mg_websocket_write(conn, 1, jc.c_str(), jc.length());
	return conn->content_len == 4 && !memcmp(conn->content, "exit", 4) ?
		MG_FALSE : MG_TRUE;
}

// Handle_post_request reply manager
// User has submitted a form, show submitted data and a variable value
// Parse form data. var1 and var2 are guaranteed to be NUL-terminated
static void send_reply_handle_post_request(struct mg_connection *conn) {
	char var1[500], var2[500];

	mg_get_var(conn, "input_1", var1, sizeof(var1));
	mg_get_var(conn, "input_2", var2, sizeof(var2));

	mg_send_header(conn, "Content-Type", "text/plain");

	// If var1 = JSON then var2 contains a JSON that can be parsed
	if (!strcmp(var1, "JSON")) {
		std::string json(var2);
		if (libjson::is_valid(json)) {
			JSONNode v2 = libjson::parse(json);
			ParseJSON(conn, v2);
		}
		else {
			mg_printf_data(conn, "Error! Expected Valid JSON [%s]\n", var2);
			silent_printf(silent, "Error! Expected Valid JSON [%s]\n", var2);
		}
	} // var1 == JSON
	else {
		// Send reply to the client, showing submitted form values.
		// POST data is in conn->content, data length is in conn->content_len
		mg_printf_data(conn,
			"Submitted data: [%.*s]\n"
			"Submitted data length: %d bytes\n"
			"input_1: [%s]\n"
			"input_2: [%s]\n",
			conn->content_len, conn->content,
			conn->content_len, var1, var2);
		silent_printf(silent,
			"Submitted data: [%.*s]\n"
			"Submitted data length: %d bytes\n"
			"input_1: [%s]\n"
			"input_2: [%s]\n",
			conn->content_len, conn->content,
			conn->content_len, var1, var2);
	} // var1 != JSON
}

// Get_request reply manager
// User has submitted a get, check for the 2 parameters.
static void send_reply_get_request(struct mg_connection *conn) {
	char var1[500], var2[500];
	JSONNode n(JSON_NODE);

	mg_get_var(conn, "input_1", var1, sizeof(var1));
	mg_get_var(conn, "input_2", var2, sizeof(var2));

	// Send reply to the client, showing parameter values.
	// Send plain text
	mg_send_header(conn, "Content-Type", "text/plain");
	mg_printf_data(conn,
		"input_1: [%s]\n"
		"input_2: [%s]\n",
		var1, var2);
	silent_printf(silent,
		"input_1: [%s]\n"
		"input_2: [%s]\n",
		var1, var2);
	// Send JSON
	n.push_back(JSONNode("input_1", var1));
	n.push_back(JSONNode("input_2", var2));
	std::string jc = n.write();
	mg_printf_data(conn, jc.c_str());
	silent_printf(silent, jc.c_str());
	silent_printf(silent, "\n");
}

// Default reply manager
// Show usage and throw error message
static mg_result send_reply_default(struct mg_connection *conn) {
	if (!strcmp(conn->uri, "/favicon.ico")) {
		return MG_TRUE;  // should be false, but can't find file
	}
	else {
		mg_send_header(conn, "Content-Type", "text/plain");
		mg_printf_data(conn,
			"Usage\n"
			"/post_request\n"
			"/get_request?input_1=value&input_2=value\n");

		silent_printf(silent, "Received invalid uri: %s\n", conn->uri);

		return MG_TRUE;
	}
}

// WebServer reply manager
// Here we need to manage the pages/events we want to deal with
static int send_reply(struct mg_connection *conn) {
	if (conn->is_websocket) {
		// This handler is called for each incoming websocket frame, one or more
		// times for connection lifetime.
		return send_reply_websocket(conn);
	} // conn->is_websocket
	else {
		if (!strcmp(conn->uri, "/handle_post_request")) {
			// User has submitted a form, show submitted data and a variable value
			// Parse form data. var1 and var2 are guaranteed to be NUL-terminated
			send_reply_handle_post_request(conn);
		} 
		else if (!strcmp(conn->uri, "/post_request")) {
			// Show HTML form.
			mg_send_data(conn, html_form, strlen(html_form));
			silent_printf(silent, "HTML form shown\n");
		}
		else if (!strcmp(conn->uri, "/get_request")) {
			// User has submitted a get, check for the parameters
			send_reply_get_request(conn);
		}
		else {
			// Default
			send_reply_default(conn);
		}
		return MG_TRUE;
	} // !conn->is_websocket
}

// WebServer callback function
static int ev_handler(struct mg_connection *conn, enum mg_event ev) {
	if (ev == MG_REQUEST) {
		return send_reply(conn);
	}
	else if (ev == MG_AUTH) {
		return MG_TRUE;
	}
	else {
		return MG_FALSE;
	}
}

// Command line argument management
bool cmdline_arg_set(struct mg_server *server, int argc, char *argv[]) {
	char listening_port[6] = "80"; // default web port
	silent = false; // default no silent mode
	//char cCurrentPath[FILENAME_MAX]; // working directory

	bool retval = true;
	//_getcwd(cCurrentPath, sizeof(cCurrentPath));

	// read argument for Console
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-Silent")) {
			silent = true;
		}
		else if (!strcmp(argv[i], "-Port")) {
			if (++i < argc) {
				char* p = 0x0;
				long lport = strtol(argv[i], &p, 10);
				if (lport <= 0 || lport > 65535) {
					silent_printf(false, "Invalid Port [%s]\n", argv[i]);
					retval = false;
				}
				else {
					strcpy_s(listening_port, argv[i]);
				}
			}
			else {
				silent_printf(false, "Missinig Port Number\n");
				retval = false;
			}
		}
		else if (!strcmp(argv[i], "-h")){
			silent_printf(false,
				"EmbeddedWebServer Usage:\n"
				"-Silent\n"
				"-Port <port_number>\n");
			retval = false;
		}
		else {
			silent_printf(false, "Unrecognized Parameter [%s]\n", argv[i]);
			retval = false;
		}
	}

	if (retval) {
		mg_set_option(server, "listening_port", listening_port);
		silent_printf(silent, "Starting on port %s\n", mg_get_option(server, "listening_port"));

		//mg_set_option(server, "root", cCurrentPath);
		//silent_printf(silent, "Root: %s\n", mg_get_option(server, "root"));
		//silent_printf(silent, "path: %s\n", cCurrentPath);
	}

	return retval;
}

int main(int argc, char *argv[]) {
	struct mg_server *server = mg_create_server(NULL, ev_handler);

	if (cmdline_arg_set(server, argc, argv)) {
		for (;;) {
			mg_poll_server(server, 1000);
		}

		mg_destroy_server(&server);

		return 0;
		}
	else {
		return -1;
	}
}