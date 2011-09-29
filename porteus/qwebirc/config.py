# qwebirc configuration file
#
# This a Python program that is imported, so feel free to use any
# Python here!
#
# Note that some changes to this configuration file require re-running
# compile.py and others require restarting qwebirc (and some require
# both!)
# If in doubt always re-compile and restart.

# The following line is required, don't remove it!
from qwebirc.config_options import *

# IRC OPTIONS
# ---------------------------------------------------------------------
#
# OPTION: IRCSERVER
#         Hostname (or IP address) of IRC server to connect to.
# OPTION: IRCPORT
#         Port of IRC server to connect to.
IRCSERVER, IRCPORT = "localhost", 6667

# OPTION: REALNAME
#         The realname field of IRC clients will be set to this value.
REALNAME = "AethyrJabber"

# OPTION: IDENT
#        ident to use on irc, possible values include:
#        - a string, e.g. IDENT = "webchat"
#        - the literal value IDENT_HEX, this will set the ident to the
#          a hexadecimal version of the users IP address, e.g
#          IDENT = IDENT_HEX
#        - the literal value IDENT_NICKNAME, this will use the users
#          supplied nickname as their ident.
IDENT = "user"

# OPTION: OUTGOING_IP
#         The IP address to bind to when connecting to the IRC server.
#
#         This will not change the IP address that qwebirc listens on. 
#         You will need to call run.py with the --ip/-i option if you 
#         want that.
#OUTGOING_IP = "127.0.0.1"

# OPTION: WEBIRC_MODE
#         This option controls how the IP/hostname of the connecting
#         browser will be sent to IRC.
#
#         Possible values include:
#         - the string "webirc", i.e. WEBIRC_MODE = "webirc"
#           Use WEBIRC type blocks, with a server configuration of
#           the following style:
#
#           cgiirc {
#             type webirc;
#             hostname <qwebirc's ip address>;
#             password <password>;
#           };
#
#           Remember to set the WEBIRC_PASSWORD value to be the
#           same as <password>.
#         - the string "cgiirc", i.e. WEBIRC_MODE = "cgiirc"
#           old style CGIIRC command, set CGIIRC_STRING to be the
#           command used to set the ip/hostname, and set
#           WEBIRC_PASSWORD to be the password used in the server's
#           configuration file.
#         - the literal value None, i.e. WEBIRC_MODE = None
#           Send the IP and hostname in the realname field, overrides
#          the REALNAME option.
WEBIRC_MODE = "webirc"

# OPTION: WEBIRC_PASSWORD
#         Used for WEBIRC_MODE webirc and cgiirc, see WEBIRC_MODE
#         option documentation.
WEBIRC_PASSWORD = ""

# OPTION: CGIIRC_STRING
#         Command sent to IRC server in for cgiirc WEBIRC_MODE.
#         See WEBIRC_MODE option documentation.
#CGIIRC_STRING = "CGIIRC"

# UI OPTIONS
# ---------------------------------------------------------------------
#
# OPTION: BASE_URL
#         URL that this qwebirc instance will be available at, add the
#         port number if your instance runs on a port other than 80.
BASE_URL = "http://localhost/"

# OPTION: NETWORK_NAME
#         The name of your IRC network, displayed throughout the
#         application.
NETWORK_NAME = "Byzantium Mesh"

# OPTION: APP_TITLE
#         The title of the application in the web browser.
APP_TITLE = NETWORK_NAME + " Chat"

# NICKNAME VALIDATION OPTIONS
# ---------------------------------------------------------------------
#
# OPTION: NICKNAME_VALIDATE
#         If True then user nicknames will be validated according to
#         the configuration below, otherwise they will be passed
#         directly to the ircd.
NICKNAME_VALIDATE = True

# OPTION: NICKNAME_VALID_FIRST_CHAR
#         A string containing valid characters for the first letter of
#         a nickname.
#         Default is as in RFC1459.
import string
NICKNAME_VALID_FIRST_CHAR = string.letters + "_[]{}`^\\|"

# OPTION: NICKNAME_VALID_SUBSEQUENT_CHAR
#         A string containing valid characters for the rest of the
#         nickname.
NICKNAME_VALID_SUBSEQUENT_CHARS = NICKNAME_VALID_FIRST_CHAR + string.digits + "-"

# OPTION: NICKNAME_MINIMUM_LENGTH
#         Minimum characters permitted in a nickname on your network.
NICKNAME_MINIMUM_LENGTH = 2

# OPTION: NICKNAME_MAXIMUM_LENGTH
#         Maximum characters permitted in a nickname on your network.
#         Ideally we'd extract this from the ircd, but we need to know
#         before we connect.
NICKNAME_MAXIMUM_LENGTH = 15

# FEEDBACK OPTIONS
# ---------------------------------------------------------------------
#
# These options control the feedback module, which allows users to
# send feedback directly from qwebirc (via email).
#
# OPTION: FEEDBACK_FROM
#         E-mail address that feedback will originate from.
#FEEDBACK_FROM = "moo@moo.com"

# OPTION: FEEDBACK_TO:
#         E-mail address that feedback will be sent to.
#FEEDBACK_TO = "moo@moo.com"

# OPTION: FEEDBACK_SMTP_HOST
#         Hostname/IP address of SMTP server feedback will be sent
#         through.
# OPTION: FEEDBACK_SMTP_PORT
#         Port of SMTP server feedback will be sent through.
#FEEDBACK_SMTP_HOST, FEEDBACK_SMTP_PORT = "127.0.0.1", 25

# ADMIN ENGINE OPTIONS
# ---------------------------------------------------------------------
#
# OPTION: ADMIN_ENGINE_HOSTS:
#         List of IP addresses to allow onto the admin engine at
#         http://instance/adminengine
ADMIN_ENGINE_HOSTS = ["127.0.0.1"]

# PROXY OPTIONS
# ---------------------------------------------------------------------
#
# OPTION: FORWARDED_FOR_HEADER
#         If you're using a proxy that passes through a forwarded-for
#         header set this option to the header name, also set
#         FORWARDED_FOR_IPS.
#FORWARDED_FOR_HEADER="x-forwarded-for"
 
# OPTION: FORWARDED_FOR_IPS
#         This option specifies the IP addresses that forwarded-for
#         headers will be accepted from.
#FORWARDED_FOR_IPS=["127.0.0.1"]

# EXECUTION OPTIONS
# ---------------------------------------------------------------------
#
# OPTION: ARGS (optional)
#         These arguments will be used as if qwebirc was run directly
#         with them, see run.py --help for a list of options.
#ARGS = "-n -p 3989"

# OPTION: SYSLOG_ADDR (optional)
#         Used in conjunction with util/syslog.py and -s option.
#         This option specifies the address and port that syslog
#         datagrams will be sent to.
#SYSLOG_ADDR = "127.0.0.1", 514

# TUNEABLE VALUES
# ---------------------------------------------------------------------
#
# You probably don't want to fiddle with these unless you really know
# what you're doing...

# OPTION: UPDATE_FREQ
#         Maximum rate (in seconds) at which updates will be propagated
#         to clients
UPDATE_FREQ = 1.0

# OPTION: MAXBUFLEN
#         Maximum client AJAX recieve buffer size (in bytes), if this
#         buffer size is exceeded then the client will be disconnected.
#         This value should match the client sendq size in your ircd's
#         configuration.
MAXBUFLEN = 100000

# OPTION: MAXSUBSCRIPTIONS
#         Maximum amount of 'subscriptions' to a specific AJAX channel,
#         i.e. an IRC connection.
#         In theory with a value greater than one you can connect more
#         than one web IRC client to the same IRC connection, ala
#         irssi-proxy.
MAXSUBSCRIPTIONS = 1

# OPTION: MAXLINELEN
#         If the client sends a line greater than MAXLINELEN (in bytes)
#         then they will be disconnected.
#         Note that IRC normally silently drops messages >=512 bytes.
MAXLINELEN = 600

# OPTION: DNS_TIMEOUT
#         DNS requests that do not respond within DNS_TIMEOUT seconds
#         will be cancelled.
DNS_TIMEOUT = 20

# OPTION: HTTP_AJAX_REQUEST_TIMEOUT
#         Connections made to the AJAX engine are closed after this
#         this many seconds.
#         Note that this value is intimately linked with the client
#         AJAX code at this time, changing it will result in bad
#         things happening.
HTTP_AJAX_REQUEST_TIMEOUT = 30

# OPTION: HTTP_REQUEST_TIMEOUT
#         Connections made to everything but the AJAX engine will
#         be closed after this many seconds, including connections
#         that haven't started/completed an HTTP request.
HTTP_REQUEST_TIMEOUT = 5

# OPTION: STATIC_BASE_URL
#         This value is used to build the URL for all static HTTP
#         requests.
#         You'd find this useful if you're running multiple qwebirc
#         instances on the same host.
STATIC_BASE_URL = ""

# OPTION: DYNAMIC_BASE_URL
#         This value is used to build the URL for all dynamic HTTP
#         requests.
#         You'd find this useful if you're running multiple qwebirc
#         instances on the same host.
DYNAMIC_BASE_URL = ""

# OPTION: CONNECTION_RESOLVER
#         A list of (ip, port) tuples of resolvers to use for looking
#         the SRV record(s) used for connecting to the name set in
#         IRC_SERVER.
#         The default value is None, and in this case qwebirc will use
#         the system's default resolver(s).
CONNECTION_RESOLVER = None

# QUAKENET SPECIFIC VALUES
# ---------------------------------------------------------------------
#
# These values are of no interest if you're not QuakeNet.
# At present they still need to be set, this will change soon.
#
# OPTION: HMACKEY
#         Shared key to use with hmac WEBIRC_MODE.
HMACKEY = "mrmoo"

# OPTION: HMACTEMPORAL
#         Divisor used for modulo HMAC timestamp generation.
HMACTEMPORAL = 30

# OPTION: AUTHGATEDOMAIN
#         Domain accepted inside authgate tickets.
AUTHGATEDOMAIN = "webchat_test"

# OPTION: QTICKETKEY
#         Key shared with the authgate that is used to decrypt
#         qtickets.
QTICKETKEY = "boo"

# OPTION: AUTH_SERVICE
#         Service that auth commands are sent to. Also used to check
#         responses from said service.
AUTH_SERVICE = "Q!TheQBot@CServe.quakenet.org"

# OPTION: AUTH_OK_REGEX
#         JavaScript regular expression that should match when
#         AUTH_SERVICE has returned an acceptable response to
#         authentication.
AUTH_OK_REGEX = "^You are now logged in as [^ ]+\\.$"

# OPTION: AUTHGATEPROVIDER
#         Authgate module to use, normally imported directly.
#         dummyauthgate does nothing.
import dummyauthgate as AUTHGATEPROVIDER
