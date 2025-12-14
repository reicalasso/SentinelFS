/*
 * Zer0 YARA Rules
 * Advanced malware detection signatures for SentinelFS
 */

// ============================================================================
// RANSOMWARE DETECTION
// ============================================================================

rule Ransomware_Generic_Note {
    meta:
        description = "Detects generic ransomware ransom notes"
        author = "Zer0"
        severity = "critical"
        category = "ransomware"
    strings:
        $ransom1 = "your files have been encrypted" ascii wide nocase
        $ransom2 = "bitcoin" ascii wide nocase
        $ransom3 = "decrypt" ascii wide nocase
        $ransom4 = "ransom" ascii wide nocase
        $ransom5 = "pay" ascii wide nocase
        $ransom6 = "wallet" ascii wide nocase
        $ransom7 = "recover your files" ascii wide nocase
        $ransom8 = "private key" ascii wide nocase
        $ext1 = ".locked" ascii wide
        $ext2 = ".encrypted" ascii wide
        $ext3 = ".crypted" ascii wide
    condition:
        (3 of ($ransom*)) or (any of ($ext*) and 2 of ($ransom*))
}

rule Ransomware_Encryption_Routine {
    meta:
        description = "Detects ransomware encryption patterns in code"
        author = "Zer0"
        severity = "critical"
        category = "ransomware"
    strings:
        $crypto1 = "AES" ascii wide nocase
        $crypto2 = "RSA" ascii wide nocase
        $crypto3 = "Fernet" ascii wide nocase
        $crypto4 = "encrypt" ascii wide nocase
        $file1 = "os.walk" ascii wide
        $file2 = "glob" ascii wide
        $file3 = "FindFirstFile" ascii wide
        $ext1 = ".doc" ascii wide nocase
        $ext2 = ".pdf" ascii wide nocase
        $ext3 = ".jpg" ascii wide nocase
        $ext4 = ".xlsx" ascii wide nocase
        $shadow1 = "vssadmin" ascii wide nocase
        $shadow2 = "shadow" ascii wide nocase
        $shadow3 = "wmic shadowcopy" ascii wide nocase
    condition:
        (2 of ($crypto*)) and (any of ($file*)) and (2 of ($ext*)) or (any of ($shadow*))
}

// ============================================================================
// BACKDOOR DETECTION
// ============================================================================

rule Backdoor_ReverseShell_C {
    meta:
        description = "Detects C/C++ reverse shell patterns"
        author = "Zer0"
        severity = "critical"
        category = "backdoor"
    strings:
        $socket1 = "socket(" ascii wide
        $socket2 = "connect(" ascii wide
        $socket3 = "AF_INET" ascii wide
        $socket4 = "SOCK_STREAM" ascii wide
        $shell1 = "/bin/sh" ascii wide
        $shell2 = "/bin/bash" ascii wide
        $shell3 = "execve" ascii wide
        $shell4 = "dup2" ascii wide
        $shell5 = "spawn_shell" ascii wide nocase
        $net1 = "sockaddr_in" ascii wide
        $net2 = "htons" ascii wide
        $net3 = "inet_pton" ascii wide
    condition:
        (3 of ($socket*)) and (2 of ($shell*)) or (all of ($net*) and any of ($shell*))
}

rule Backdoor_ReverseShell_Generic {
    meta:
        description = "Detects generic reverse shell patterns"
        author = "Zer0"
        severity = "critical"
        category = "backdoor"
    strings:
        $rs1 = "reverse shell" ascii wide nocase
        $rs2 = "reverse_shell" ascii wide nocase
        $rs3 = "bind shell" ascii wide nocase
        $rs4 = "nc -e" ascii wide
        $rs5 = "netcat" ascii wide nocase
        $rs6 = "/dev/tcp/" ascii wide
        $rs7 = "bash -i" ascii wide
        $rs8 = "sh -i" ascii wide
        $c2_1 = "C2_HOST" ascii wide
        $c2_2 = "C2_PORT" ascii wide
        $c2_3 = "c2_socket" ascii wide nocase
        $c2_4 = "connect_c2" ascii wide nocase
    condition:
        any of ($rs*) or (2 of ($c2*))
}

// ============================================================================
// ROOTKIT DETECTION
// ============================================================================

rule Rootkit_Linux_Kernel {
    meta:
        description = "Detects Linux kernel rootkit patterns"
        author = "Zer0"
        severity = "critical"
        category = "rootkit"
    strings:
        $lkm1 = "init_module" ascii
        $lkm2 = "cleanup_module" ascii
        $lkm3 = "module_init" ascii
        $lkm4 = "module_exit" ascii
        $hook1 = "sys_call_table" ascii
        $hook2 = "kallsyms" ascii
        $hook3 = "orig_getdents" ascii wide nocase
        $hook4 = "hooked_" ascii wide nocase
        $hide1 = "hide_pid" ascii
        $hide2 = "hide_file" ascii
        $hide3 = "hidden_" ascii wide nocase
        $hide4 = "HIDDEN_" ascii wide
        $wp1 = "write_cr0" ascii
        $wp2 = "disable_wp" ascii wide nocase
        $wp3 = "enable_wp" ascii wide nocase
    condition:
        (2 of ($lkm*)) and ((any of ($hook*)) or (any of ($hide*)) or (any of ($wp*)))
}

rule Rootkit_Syscall_Hook {
    meta:
        description = "Detects syscall hooking patterns"
        author = "Zer0"
        severity = "critical"
        category = "rootkit"
    strings:
        $syscall1 = "__NR_getdents" ascii
        $syscall2 = "__NR_getdents64" ascii
        $syscall3 = "__NR_read" ascii
        $syscall4 = "__NR_write" ascii
        $syscall5 = "__NR_kill" ascii
        $hook1 = "orig_" ascii
        $hook2 = "hooked_" ascii
        $hook3 = "asmlinkage" ascii
        $proc1 = "/proc" ascii
        $proc2 = "LD_PRELOAD" ascii
    condition:
        (2 of ($syscall*)) and (2 of ($hook*)) or ($proc2)
}

// ============================================================================
// CRYPTOMINER DETECTION
// ============================================================================

rule Cryptominer_Config {
    meta:
        description = "Detects cryptocurrency miner configuration"
        author = "Zer0"
        severity = "high"
        category = "cryptominer"
    strings:
        $pool1 = "stratum+tcp://" ascii wide
        $pool2 = "stratum+ssl://" ascii wide
        $pool3 = "pool.minergate" ascii wide
        $pool4 = "minexmr" ascii wide nocase
        $pool5 = "xmrpool" ascii wide nocase
        $pool6 = "nanopool" ascii wide nocase
        $coin1 = "monero" ascii wide nocase
        $coin2 = "bitcoin" ascii wide nocase
        $coin3 = "ethereum" ascii wide nocase
        $miner1 = "xmrig" ascii wide nocase
        $miner2 = "cryptonight" ascii wide nocase
        $miner3 = "hashrate" ascii wide nocase
        $miner4 = "RandomX" ascii wide
        $config1 = "donate-level" ascii wide
        $config2 = "cpu" ascii wide
        $config3 = "huge-pages" ascii wide
        $config4 = "rig-id" ascii wide
    condition:
        (any of ($pool*)) or ((any of ($coin*)) and (any of ($miner*))) or (3 of ($config*))
}

rule Cryptominer_Dropper {
    meta:
        description = "Detects cryptominer dropper scripts"
        author = "Zer0"
        severity = "high"
        category = "cryptominer"
    strings:
        $kill1 = "pkill" ascii
        $kill2 = "killall" ascii
        $kill3 = "xmrig" ascii wide nocase
        $kill4 = "minerd" ascii wide nocase
        $hide1 = "/tmp/." ascii
        $hide2 = ".X11-unix" ascii
        $hide3 = "kworker" ascii
        $cron1 = "crontab" ascii
        $cron2 = "/var/spool/cron" ascii
        $cron3 = "@reboot" ascii
        $dl1 = "curl" ascii
        $dl2 = "wget" ascii
    condition:
        (2 of ($kill*)) or ((any of ($hide*)) and (any of ($cron*))) or ((any of ($dl*)) and (any of ($hide*)))
}

// ============================================================================
// TROJAN DETECTION
// ============================================================================

rule Trojan_RAT {
    meta:
        description = "Detects Remote Access Trojan patterns"
        author = "Zer0"
        severity = "critical"
        category = "trojan"
    strings:
        $cmd1 = "cmd.exe" ascii wide
        $cmd2 = "/bin/sh" ascii wide
        $cmd3 = "/bin/bash" ascii wide
        $net1 = "socket" ascii wide
        $net2 = "connect" ascii wide
        $net3 = "bind" ascii wide
        $rat1 = "webcam" ascii wide nocase
        $rat2 = "screenshot" ascii wide nocase
        $rat3 = "keylog" ascii wide nocase
        $rat4 = "remote" ascii wide nocase
        $beacon1 = "beacon" ascii wide nocase
        $beacon2 = "BEACON_INTERVAL" ascii wide
        $c2_1 = "C2_SERVER" ascii wide
        $c2_2 = "c2_socket" ascii wide nocase
    condition:
        (any of ($cmd*)) and (2 of ($net*)) and (any of ($rat*)) or (any of ($beacon*)) or (any of ($c2*))
}

rule Trojan_FakeUpdate {
    meta:
        description = "Detects fake update trojan patterns"
        author = "Zer0"
        severity = "high"
        category = "trojan"
    strings:
        $fake1 = "fake" ascii wide nocase
        $fake2 = "update" ascii wide nocase
        $fake3 = "system update" ascii wide nocase
        $fake4 = "security update" ascii wide nocase
        $distract1 = "please wait" ascii wide nocase
        $distract2 = "installing" ascii wide nocase
        $distract3 = "progress" ascii wide nocase
        $steal1 = "steal" ascii wide nocase
        $steal2 = "exfiltrate" ascii wide nocase
        $steal3 = "credential" ascii wide nocase
        $c2_1 = "C2_SERVERS" ascii wide
        $c2_2 = "darkweb" ascii wide nocase
        $c2_3 = ".onion" ascii wide
    condition:
        (2 of ($fake*)) and (any of ($distract*)) or (any of ($steal*) and any of ($c2*))
}

// ============================================================================
// KEYLOGGER DETECTION
// ============================================================================

rule Keylogger_Generic {
    meta:
        description = "Detects keylogger patterns"
        author = "Zer0"
        severity = "critical"
        category = "keylogger"
    strings:
        $api1 = "GetAsyncKeyState" ascii wide
        $api2 = "SetWindowsHookEx" ascii wide
        $api3 = "GetKeyboardState" ascii wide
        $api4 = "keybd_event" ascii wide
        $linux1 = "/dev/input" ascii
        $linux2 = "eventX" ascii
        $linux3 = "xdotool" ascii
        $log1 = "keylog" ascii wide nocase
        $log2 = "keystroke" ascii wide nocase
        $log3 = "capture_keys" ascii wide nocase
        $log4 = "_capture_clipboard" ascii wide nocase
        $exfil1 = "EXFIL" ascii wide
        $exfil2 = "exfiltrate" ascii wide nocase
        $exfil3 = "send_key" ascii wide nocase
    condition:
        (2 of ($api*)) or (2 of ($linux*)) or (2 of ($log*)) or (any of ($exfil*) and any of ($log*))
}

// ============================================================================
// SPYWARE DETECTION
// ============================================================================

rule Spyware_DataStealer {
    meta:
        description = "Detects data stealer/spyware patterns"
        author = "Zer0"
        severity = "critical"
        category = "spyware"
    strings:
        $browser1 = "chrome" ascii wide nocase
        $browser2 = "firefox" ascii wide nocase
        $browser3 = "Login Data" ascii wide
        $browser4 = "cookies.sqlite" ascii wide
        $cred1 = ".ssh" ascii
        $cred2 = ".aws/credentials" ascii
        $cred3 = ".kube/config" ascii
        $cred4 = ".git-credentials" ascii
        $wallet1 = "wallet.dat" ascii wide
        $wallet2 = ".bitcoin" ascii
        $wallet3 = ".ethereum" ascii
        $wallet4 = "crypto_wallets" ascii wide nocase
        $steal1 = "steal" ascii wide nocase
        $steal2 = "collect" ascii wide nocase
        $steal3 = "harvest" ascii wide nocase
        $steal4 = "exfiltrate" ascii wide nocase
        $class1 = "DataStealer" ascii wide
        $class2 = "CredentialStealer" ascii wide
    condition:
        (2 of ($browser*)) or (2 of ($cred*)) or (2 of ($wallet*)) or (any of ($steal*) and (any of ($browser*) or any of ($cred*))) or (any of ($class*))
}

// ============================================================================
// WORM DETECTION
// ============================================================================

rule Worm_Network {
    meta:
        description = "Detects network worm patterns"
        author = "Zer0"
        severity = "critical"
        category = "worm"
    strings:
        $scan1 = "scan_port" ascii wide nocase
        $scan2 = "scan_network" ascii wide nocase
        $scan3 = "SCAN_PORTS" ascii wide
        $prop1 = "propagate" ascii wide nocase
        $prop2 = "spread" ascii wide nocase
        $prop3 = "infect" ascii wide nocase
        $prop4 = "infected_hosts" ascii wide nocase
        $brute1 = "brute" ascii wide nocase
        $brute2 = "CREDENTIALS" ascii wide
        $brute3 = "password" ascii wide nocase
        $exploit1 = "exploit" ascii wide nocase
        $exploit2 = "EternalBlue" ascii wide nocase
        $class1 = "NetworkWorm" ascii wide
        $class2 = "Worm" ascii wide
    condition:
        (2 of ($scan*)) or (2 of ($prop*)) or (any of ($brute*) and any of ($scan*)) or (any of ($class*))
}

// ============================================================================
// POWERSHELL MALWARE
// ============================================================================

rule PowerShell_Dropper {
    meta:
        description = "Detects malicious PowerShell dropper patterns"
        author = "Zer0"
        severity = "critical"
        category = "dropper"
    strings:
        $enc1 = "-EncodedCommand" ascii wide nocase
        $enc2 = "-enc " ascii wide nocase
        $enc3 = "FromBase64String" ascii wide
        $bypass1 = "-ExecutionPolicy Bypass" ascii wide nocase
        $bypass2 = "-ep bypass" ascii wide nocase
        $bypass3 = "Set-ExecutionPolicy" ascii wide nocase
        $amsi1 = "AmsiUtils" ascii wide
        $amsi2 = "amsiContext" ascii wide nocase
        $amsi3 = "AMSI" ascii wide
        $download1 = "DownloadString" ascii wide
        $download2 = "DownloadFile" ascii wide
        $download3 = "Invoke-WebRequest" ascii wide
        $iex1 = "IEX" ascii wide
        $iex2 = "Invoke-Expression" ascii wide
        $disable1 = "DisableRealtimeMonitoring" ascii wide
        $disable2 = "Set-MpPreference" ascii wide
        $persist1 = "ScheduledTask" ascii wide
        $persist2 = "WmiInstance" ascii wide
        $persist3 = "Registry" ascii wide nocase
    condition:
        (any of ($enc*)) or (any of ($bypass*) and any of ($download*)) or (any of ($amsi*)) or (any of ($iex*) and any of ($download*)) or (any of ($disable*)) or (2 of ($persist*))
}

// ============================================================================
// LINUX PERSISTENCE
// ============================================================================

rule Linux_Persistence {
    meta:
        description = "Detects Linux persistence mechanisms"
        author = "Zer0"
        severity = "high"
        category = "persistence"
    strings:
        $cron1 = "crontab" ascii
        $cron2 = "/etc/cron" ascii
        $cron3 = "@reboot" ascii
        $rc1 = "/etc/rc.local" ascii
        $rc2 = "rc.d" ascii
        $systemd1 = "systemctl enable" ascii
        $systemd2 = "/etc/systemd/system" ascii
        $systemd3 = ".service" ascii
        $profile1 = ".bashrc" ascii
        $profile2 = ".bash_profile" ascii
        $profile3 = ".profile" ascii
        $profile4 = ".zshrc" ascii
        $autostart1 = "autostart" ascii wide nocase
        $autostart2 = ".desktop" ascii
        $ssh1 = "authorized_keys" ascii
        $ld1 = "ld.so.preload" ascii
        $ld2 = "LD_PRELOAD" ascii
        $func1 = "install_persistence" ascii wide nocase
        $func2 = "create_persistence" ascii wide nocase
    condition:
        (3 of ($cron*, $rc*, $systemd*)) or (3 of ($profile*)) or (any of ($autostart*) and any of ($profile*)) or (any of ($ssh1, $ld*)) or (any of ($func*))
}

// ============================================================================
// SHELL SCRIPTS MALWARE
// ============================================================================

rule Shell_Malicious {
    meta:
        description = "Detects malicious shell script patterns"
        author = "Zer0"
        severity = "high"
        category = "script"
    strings:
        $shebang1 = "#!/bin/bash" ascii
        $shebang2 = "#!/bin/sh" ascii
        $curl1 = "curl" ascii
        $wget1 = "wget" ascii
        $pipe1 = "| bash" ascii
        $pipe2 = "| sh" ascii
        $rm1 = "rm -rf /" ascii
        $rm2 = "rm -rf ~" ascii
        $chmod1 = "chmod 777" ascii
        $chmod2 = "chmod +x" ascii
        $hidden1 = "/tmp/." ascii
        $hidden2 = "mkdir -p /tmp/." ascii
        $disable1 = "systemctl stop" ascii
        $disable2 = "iptables -F" ascii
        $disable3 = "ufw disable" ascii
        $clean1 = "history -c" ascii
        $clean2 = "unset HISTFILE" ascii
        $clean3 = "/dev/null" ascii
    condition:
        (any of ($shebang*)) and (((any of ($curl1, $wget1)) and (any of ($pipe*))) or (any of ($rm*)) or (2 of ($hidden*)) or (2 of ($disable*)) or (2 of ($clean*)))
}

// ============================================================================
// GENERIC SUSPICIOUS PATTERNS
// ============================================================================

rule Suspicious_Exfiltration {
    meta:
        description = "Detects data exfiltration patterns"
        author = "Zer0"
        severity = "high"
        category = "exfiltration"
    strings:
        $exfil1 = "exfil" ascii wide nocase
        $exfil2 = "EXFIL_" ascii wide
        $exfil3 = "exfiltrate" ascii wide nocase
        $staging1 = "STAGING" ascii wide
        $staging2 = "staging" ascii wide nocase
        $send1 = "send_data" ascii wide nocase
        $send2 = "upload" ascii wide nocase
        $encode1 = "base64" ascii wide nocase
        $encode2 = "encode" ascii wide nocase
        $dns1 = "dns exfil" ascii wide nocase
        $dns2 = "Resolve-DnsName" ascii wide
    condition:
        (2 of ($exfil*)) or (any of ($staging*) and any of ($send*)) or (any of ($dns*))
}

rule Suspicious_AntiAnalysis {
    meta:
        description = "Detects anti-analysis techniques"
        author = "Zer0"
        severity = "medium"
        category = "evasion"
    strings:
        $debug1 = "ptrace" ascii wide
        $debug2 = "IsDebuggerPresent" ascii wide
        $debug3 = "anti_debug" ascii wide nocase
        $vm1 = "VMware" ascii wide
        $vm2 = "VirtualBox" ascii wide
        $vm3 = "QEMU" ascii wide
        $sandbox1 = "sandbox" ascii wide nocase
        $sandbox2 = "analysis" ascii wide nocase
        $sleep1 = "sleep" ascii wide
        $sleep2 = "delay" ascii wide nocase
    condition:
        (any of ($debug*)) or (2 of ($vm*)) or (any of ($sandbox*) and any of ($sleep*))
}
