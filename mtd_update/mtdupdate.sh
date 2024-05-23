#!/bin/sh

#this script will call underlying update script to update given mtd partition
#the script will call setup and clean bash scripts if they exist
#in order to toggle potential gpio's that will expose the connections
#script will also use busctl to send messages to the Task so the
#user can be informed of fw update status.

run_command() {
    local cmd_output
    cmd_output="$($1 2>&1)"
    exit_code=$?
    echo "$cmd_output"
    return $exit_code
}

if [ $# -ne 2 ]; then
    exit 1
fi

setup_file="/usr/bin/setup_$2.sh"
cleanup_file="/usr/bin/cleanup_$2.sh"

#send fw update task a message that update has started
busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} Update.1.0.TransferringToComponent xyz.openbmc_project.Logging.Entry.Level.Informational 3 'REDFISH_MESSAGE_ARGS' "$2,MTD TARGET" 'REDFISH_MESSAGE_ID' 'Update.1.0.TransferringToComponent' 'namespace' 'FWUpdate'

if [ -f "$setup_file" ] && [ -x "$setup_file" ]; then
    result=$("$setup_file")
    exit_code=$?

    if [ $exit_code -ne 0 ]; then
        #send fw update task a message that update has failed
        busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} org.open_power.Logging.Error.TestError1 xyz.openbmc_project.Logging.Entry.Level.Warning 3 'REDFISH_MESSAGE_ARGS' "${result}" 'REDFISH_MESSAGE_ID' 'ResourceEvent.1.0.ResourceErrorsDetected' 'namespace' 'FWUpdate'
        if [ -f "$cleanup_file" ] && [ -x "$cleanup_file" ]; then
            "$cleanup_file"
        fi
        exit $exit_code
    fi
fi

tail -c +4097 $1 > /run/initramfs/image-$2
cd /run/initramfs
result=$(run_command "./update")
exit_code=$?

if [ $exit_code -ne 0 ]; then
    #send fw update task a message that update has failed
    busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} org.open_power.Logging.Error.TestError1 xyz.openbmc_project.Logging.Entry.Level.Warning 3 'REDFISH_MESSAGE_ARGS' "${result}" 'REDFISH_MESSAGE_ID' 'ResourceEvent.1.0.ResourceErrorsDetected' 'namespace' 'FWUpdate'
    if [ -f "$cleanup_file" ] && [ -x "$cleanup_file" ]; then
        "$cleanup_file"
    fi
    exit $exit_code
fi

if [ -f "$cleanup_file" ] && [ -x "$cleanup_file" ]; then
    "$cleanup_file"
fi

#send fw update task a message that update was success
busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} Update.1.0.UpdateSuccessful xyz.openbmc_project.Logging.Entry.Level.Informational 3 'REDFISH_MESSAGE_ARGS' "MTD TARGET,$2" 'REDFISH_MESSAGE_ID' 'Update.1.0.UpdateSuccessful' 'namespace' 'FWUpdate'
exit $exit_code

