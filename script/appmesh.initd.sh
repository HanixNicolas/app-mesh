#!/bin/bash
################################################################################
## init.d service definition file
## will be used when systemd is not installed on Linux
## https://gist.github.com/mrowe/8b617a8b12a6248d48b8
################################################################################

### BEGIN INIT INFO
#
# Provides:  appsvc
# Required-Start:   $local_fs  $remote_fs
# Required-Stop:	$local_fs  $remote_fs
# Default-Start:	2 3 4 5
# Default-Stop:	 	0 1 6
# Short-Description:	initscript
# Description:  This file should be used to construct scripts to be placed in /etc/init.d.
#
### END INIT INFO

## Fill in name of program here.
PROG="appsvc"
PROG_WATCHDOG="appmesh-entrypoint.sh"
PROGC="appc"
PROG_PATH="/opt/appmesh" ## Not need, but sometimes helpful (if $PROG resides in /opt for example).
PROG_ARGS=""
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/opt/appmesh/lib64:/usr/local/lib64:/usr/local/lib/

log() {
	logger "[$(date)]""$1"
	echo $1
}

start() {
	APPMESH_PROC_NUMBER=$(ps aux | grep -w ${PROG_PATH}/${PROG} | grep -v grep | wc -l)
	WATCHDOG_PROC_NUMBER=$(ps aux | grep -w ${PROG_PATH}/script/${PROG_WATCHDOG} | grep -v grep | wc -l)
	#echo "APPMESH_PROC_NUMBER:${APPMESH_PROC_NUMBER}  WATCHDOG_PROC_NUMBER:${WATCHDOG_PROC_NUMBER}"
	if [ "${APPMESH_PROC_NUMBER}" = "1" -a "${WATCHDOG_PROC_NUMBER}" = "1" ]; then
		## Program is running, exit with error.
		log "Error! $PROG is currently running!"
		exit 1
	else
		if [ "${WATCHDOG_PROC_NUMBER}" -ge "1" ]; then
			ps aux | grep -w ${PROG_PATH}/${PROG} | grep -v grep | awk '{print $2}' | xargs kill -9
		fi
		cd $PROG_PATH
		## Change from /dev/null to something like /var/log/$PROG if you want to save output.
		$PROG_PATH/script/$PROG_WATCHDOG &
	fi
}

stop() {
	log "Begin stop $PROG"
	APPMESH_PROC_NUMBER=$(ps aux | grep -w ${PROG_PATH}/${PROG} | grep -v grep | wc -l)
	WATCHDOG_PROC_NUMBER=$(ps aux | grep -w ${PROG_PATH}/script/${PROG_WATCHDOG} | grep -v grep | wc -l)
	#echo "APPMESH_PROC_NUMBER:${APPMESH_PROC_NUMBER}  WATCHDOG_PROC_NUMBER:${WATCHDOG_PROC_NUMBER}"
	if [ "${WATCHDOG_PROC_NUMBER}" -ge "1" ]; then
		ps aux | grep -w ${PROG_PATH}/script/${PROG_WATCHDOG} | grep -v grep | awk '{print $2}' | xargs kill -9
	fi
	if [ "${APPMESH_PROC_NUMBER}" -ge "1" ]; then
		ps aux | grep -w ${PROG_PATH}/${PROG} | grep -v grep | awk '{print $2}' | xargs kill -9
		log "$PROG stopped"
	else
		log "$PROG not started"
	fi
}

## Check to see if we are running as root first.
## Found at http://www.cyberciti.biz/tips/shell-root-user-check-script.html
if [ "$(id -u)" != "0" ]; then
	log "This script must be run as root"
	exit 1
fi
case "$1" in
start)
	start
	exit 0
	;;
stop)
	stop
	exit 0
	;;
reload | restart | force-reload)
	stop
	start
	exit 0
	;;
status)
	exit 0
	;;
**)
	echo "Usage: $0 {start|stop|reload}" 1>&2
	exit 1
	;;
esac
