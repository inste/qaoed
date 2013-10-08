
char *keywords[] = {
     "device",
     "default",
     "acl",
     "slot",
     "shelf",
     "interface",
     "target",
     "filename",
     "access-list",
     "name",
     "policy",
     "accept",
     "reject",
     "writecache",
     "broadcast",
     "log-level",
     "syslog-level",
     "syslog-facility",
     "persistent-cfg",
     "write",
     "read",
     "cfgset",
     "cfgread",
     "discover",
     "logging",
     "log",
     "type",
     "mtu",
     "log",
     "if",
     "=",
     "{",
     "}",
     ";",
     "include",
     "apisocket"
};

#define KMAX (sizeof(keywords)/sizeof(char *))

char *datatype[] = {
  "default", "b",
  "access-list","b",
  "device","b",
  "acl", "b",
  "logging", "b",
  "writecache", "%s;",
  "slot","%d;",
  "shelf","%d;",
  "mtu","%s;",
  "target","%s;",
  "cfgread","%s;",
  "read","%s;",
  "type","%s;",
  "if","%s;",
  "log","%s;",
  "write","%s;",
  "filename","%s;",
  "discover","%s;",
  "cfgset","%s;",
  "writecache","%s;",
  "log-level","%s;",
  "syslog-level","%s;",
  "syslog-facility","%s;",
  "persistent-cfg","%s;",
  "broadcast","%s;",
  "apisocket","%s;",
  "include", "%s;"
};

#define DMAX ((sizeof(datatype)/sizeof(char *)))

char *arglist[] = {
  "writecache", "on|off",
  "type", "syslog|file",
  "broadcast","on|off",
  "policy","accept|reject",
  "log-level","debug|error|info|none",
  "syslog-level","LOG_EMERG|LOG_ALERT|LOG_CRIT|LOG_ERR|LOG_WARNING|LOG_NOTICE|LOG_INFO|LOG_DEBUG",
  "syslog-facility","LOG_DAEMON|LOG_LOCAL0|LOG_LOCAL1|LOG_LOCAL2|LOG_LOCAL3|LOG_LOCAL4|LOG_LOCAL5|LOG_LOCAL6|LOG_LOCAL7|LOG_USER"
};

#define AMAX ((sizeof(arglist)/sizeof(char *)))
