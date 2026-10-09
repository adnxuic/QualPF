function Component()
{
    // default constructor
}

Component.prototype.createOperations = function()
{
    // Add the desktop and start menu shortcuts.
    component.createOperations();
    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut",
                               "@TargetDir@/bin/@QUALPF_EXECUTABLE_NAME@.exe",
                               "@DesktopDir@/@QUALPF_DISPLAY_NAME@.lnk",
                               "workingDirectory=@TargetDir@");

        component.addOperation("CreateShortcut",
                               "@TargetDir@/bin/@QUALPF_EXECUTABLE_NAME@.exe",
                               "@StartMenuDir@/@QUALPF_DISPLAY_NAME@.lnk",
                               "workingDirectory=@TargetDir@",
                               "iconPath=@TargetDir@/bin/@QUALPF_EXECUTABLE_NAME@.exe", "iconId=0",
                               "description=Start @QUALPF_DISPLAY_NAME@");

        component.addOperation("CreateShortcut",
                               "@TargetDir@/MaintenanceTool.exe",
                               "@StartMenuDir@/Uninstall.lnk",
                               "workingDirectory=@TargetDir@",
                               "iconPath=@TargetDir@/MaintenanceTool.exe", "iconId=0",
                               "description=Uninstall @QUALPF_DISPLAY_NAME@",
                               "--start-uninstaller");

        if (@QUALPF_IFW_MSVC_RUNTIME@) {
            // return value 3010 means it need a reboot, but in most cases it is not needed for running Qt application
            // return value 5100 means there's a newer version of the runtime already installed
            component.addOperation("Execute", "{0,3010,1638,5100}", "@TargetDir@\\bin\\vc_redist.x64.exe", "/quiet", "/norestart");
        }
    }
}
