
static const char *signames[] = {
    "type='signal',interface='org.freedesktop.DBus'",
    "type='signal',interface='"BLUEZ_DBUS_BASE_IFC".Adapter'",
    "type='signal',interface='"BLUEZ_DBUS_BASE_IFC".Device'",
    "type='signal',interface='"BLUEZ_DBUS_BASE_IFC".Input'",
    "type='signal',interface='"BLUEZ_DBUS_BASE_IFC".Network'",
    "type='signal',interface='"BLUEZ_DBUS_BASE_IFC".NetworkServer'",
    "type='signal',interface='"BLUEZ_DBUS_BASE_IFC".HealthDevice'",
    "type='signal',interface='org.bluez.AudioSink'",
    // these were only in removematch???
    "type='signal',interface='"BLUEZ_DBUS_BASE_IFC".AudioSink'",
    "type='signal',interface='org.bluez.audio.Manager'",
    NULL};

#define SIGNITEMS \
    SIGDEF("org.freedesktop.DBus", "NameAcquired", BSIG_NameAcquired) \
    SIGDEF("org.bluez.Adapter", "DeviceFound", BSIG_AdapterDeviceFound) \
    SIGDEF("org.bluez.Adapter", "DeviceDisappeared", BSIG_AdapterDeviceDisappeared) \
    SIGDEF("org.bluez.Adapter", "DeviceCreated", BSIG_AdapterDeviceCreated) \
    SIGDEF("org.bluez.Adapter", "DeviceRemoved", BSIG_AdapterDeviceRemoved) \
    SIGDEF("org.bluez.Adapter", "PropertyChanged", BSIG_AdapterPropertyChanged) \
    SIGDEF("org.bluez.Device", "PropertyChanged", BSIG_DevicePropertyChanged) \
    SIGDEF("org.bluez.Device", "DisconnectRequested", BSIG_DeviceDisconnectRequested) \
    SIGDEF("org.bluez.Input", "PropertyChanged", BSIG_InputDevicePropertyChanged) \
    SIGDEF("org.bluez.Network", "PropertyChanged", BSIG_PanDevicePropertyChanged) \
    SIGDEF("org.bluez.NetworkServer", "DeviceDisconnected", BSIG_NetworkDeviceDisconnected) \
    SIGDEF("org.bluez.NetworkServer", "DeviceConnected", BSIG_NetworkDeviceConnected) \
    SIGDEF("org.bluez.HealthDevice", "ChannelConnected", BSIG_HealthDeviceChannelConnected) \
    SIGDEF("org.bluez.HealthDevice", "ChannelDeleted", BSIG_HealthDeviceChannelDeleted) \
    SIGDEF("org.bluez.HealthDevice", "PropertyChanged", BSIG_HealthDevicePropertyChanged) \
    SIGDEF("org.bluez.AudioSink", "PropertyChanged", BSIG_AudioPropertyChanged)

#define METHITEMS \
    SIGDEF("org.bluez.Agent", "Cancel", BMETH_AgentCancel) \
    SIGDEF("org.bluez.Agent", "Authorize", BMETH_AgentAuthorize) \
    SIGDEF("org.bluez.Agent", "OutOfBandAvailable", BMETH_AgentOutOfBandDataAvailable) \
    SIGDEF("org.bluez.Agent", "RequestPinCode", BMETH_RequestPinCode) \
    SIGDEF("org.bluez.Agent", "RequestPasskey", BMETH_RequestPasskey) \
    SIGDEF("org.bluez.Agent", "RequestOobData", BMETH_RequestOobData) \
    SIGDEF("org.bluez.Agent", "DisplayPasskey", BMETH_DisplayPasskey) \
    SIGDEF("org.bluez.Agent", "RequestConfirmation", BMETH_RequestPasskeyConfirmation) \
    SIGDEF("org.bluez.Agent", "RequestPairingConsent", BMETH_RequestPairingConsent) \
    SIGDEF("org.bluez.Agent", "Release", BMETH_Release)

static CHARMAPTYPE bondmap[] = {
    {BLUEZ_DBUS_BASE_IFC ".Error.AuthenticationFailed", BOND_RESULT_AUTH_FAILED},
    {BLUEZ_DBUS_BASE_IFC ".Error.AuthenticationRejected", BOND_RESULT_AUTH_REJECTED},
    {BLUEZ_DBUS_BASE_IFC ".Error.AuthenticationCanceled", BOND_RESULT_AUTH_CANCELED},
    {BLUEZ_DBUS_BASE_IFC ".Error.ConnectionAttemptFailed", BOND_RESULT_REMOTE_DEVICE_DOWN},
    {BLUEZ_DBUS_BASE_IFC ".Error.AlreadyExists", BOND_RESULT_SUCCESS},
    {BLUEZ_DBUS_BASE_IFC ".Error.InProgress", BOND_RESULT_DISCOVERY_IN_PROGRESS},
    {BLUEZ_DBUS_BASE_IFC ".Error.RepeatedAttempts", BOND_RESULT_REPEATED_ATTEMPTS},
    {BLUEZ_DBUS_BASE_IFC ".Error.AuthenticationTimeout", BOND_RESULT_AUTH_TIMEOUT},
    {NULL, -1}};

static CHARMAPTYPE inputconnectmap[] = {
    {BLUEZ_ERROR_IFC ".ConnectionAttemptFailed", INPUT_CONNECT_FAILED_ATTEMPT_FAILED},
    {BLUEZ_ERROR_IFC ".AlreadyConnected", INPUT_CONNECT_FAILED_ALREADY_CONNECTED},
    {BLUEZ_ERROR_IFC ".Failed", INPUT_DISCONNECT_FAILED_NOT_CONNECTED},
    {NULL, INPUT_OPERATION_GENERIC_FAILURE}};

static CHARMAPTYPE panconnectmap[] = {
    {BLUEZ_ERROR_IFC ".ConnectionAttemptFailed", PAN_CONNECT_FAILED_ATTEMPT_FAILED},
    {BLUEZ_ERROR_IFC ".Failed", PAN_DISCONNECT_FAILED_NOT_CONNECTED},
    {NULL, PAN_OPERATION_GENERIC_FAILURE}};

static CHARMAPTYPE healthmap[] = {
    {BLUEZ_ERROR_IFC ".InvalidArgs", HEALTH_OPERATION_INVALID_ARGS},
    {BLUEZ_ERROR_IFC ".HealthError", HEALTH_OPERATION_ERROR},
    {BLUEZ_ERROR_IFC ".NotFound", HEALTH_OPERATION_NOT_FOUND},
    {BLUEZ_ERROR_IFC ".NotAllowed", HEALTH_OPERATION_NOT_ALLOWED},
    {NULL, HEALTH_OPERATION_GENERIC_FAILURE}};
