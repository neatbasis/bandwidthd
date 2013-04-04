%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "conf.tab.h"

%}

%%
[ \t]+            /* Ignore */;
\n            { return TOKNEWLINE; }
#.*\n              { return TOKNEWLINE; }
([0-9]{1,3}"."){3,3}([0-9]{1,3}) { bdconfig_lval.string=strdup(yytext); return IPADDR; }
[-0-9]+          { bdconfig_lval.number=atoi(yytext); return NUMBER; }
\".*\"          { bdconfig_lval.string=strdup(yytext); return STRING; }
\/            { return TOKSLASH; }
true|false              { bdconfig_lval.number=!strcmp(yytext,"true"); return STATE; }
subnet          { return TOKSUBNET; }
notsubnet         { return TOKNOTSUBNET; }
dev            { return TOKDEV; }
skip_intervals      { return TOKSKIPINTERVALS; }
graph_cutoff      { return TOKGRAPHCUTOFF; }
promiscuous        { return TOKPROMISC; }
output_cdf        { return TOKOUTPUTCDF; }    
recover_cdf        { return TOKRECOVERCDF; }    
graph          { return TOKGRAPH; }
filter          { return TOKFILTER; }
meta_refresh      { return TOKMETAREFRESH; }
pgsql_connect_string  { return TOKPGSQLCONNECTSTRING; }
sqlite_filename      { return TOKSQLITEFILENAME; }
sensor_id        { return TOKSENSORID; }
htdocs_dir        { return TOKHTDOCSDIR; }
log_dir          { return TOKLOGDIR; }
description        { return TOKDESCRIPTION; }
management_url      { return TOKMANAGEMENTURL; }
extensions        { return TOKEXTENSIONS; }
.            { return TOKJUNK; }
%%
