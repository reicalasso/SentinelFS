/*
 * Zer0 Advanced malware detection signatures
 */

//============================================================================
// RANSOMWARE DETECTION
// ============================================================================

rule Ransomware_Generic_Encryption {
    meta:
        description = "Detects generic ransomware encryption patterns"
        author = "Zer0"
        severity = "critical"
        category = "ransomware"
    strings:
        $encrypt1 = "AES" ascii wide nocase
        $encrypt2 = "RSA" ascii wide nocase
        $encrypt3 = "encrypt" ascii wide nocase
        $random1 = "bitcoin" ascii wide nocase
        $random2 = "ransom" ascii wide nocase
        $random3 = "decrypt" ascii wide nocase
        $random4 = "your files" ascii wide nocase
        $random5 = "pay" ascii wide nocase
        $ext1 = ".locked" ascii wide
        $ext2 = ".crypted" ascii wide
        $ext3 = ".crypt" ascii wide
    condition:
        (2 of ($encrypt*)) and (2 of ($random*)) or (any of ($ext*))
}

rule Ransomware_WannaCry {
    meta:
        description = "Detects WannaCry ransomware"
        author = "Zer0"
        severity = "critical"
        category = "ransomware"
    strings:
        $s1 = "WNcry@2o7" ascii wide
        $s2 = "WanaDecryptor" ascii wide
        $s3 = ".WNCRY" ascii wide
        $s4 = "tasksch.exe" ascii wide
    condition:
        any of them
}

rule Ransomware_Locky {
    meta:
        description = "Detects Locky ransomware variants"
        author = "Zer0"
        severity = "critical"
        category = "ransomware"
    strings:
        $s1 = ".locky" ascii wide
        $s2 = ".zepto" ascii wide
        $s3 = ".odin" ascii wide
        $s4 = "_HELP_instructions" ascii wide
    condition:
        any of them
}

rule Ransomware_CryptoLocker {
    meta:
        description = "Detects CryptoLocker ransomware"
        author = "Zer0"
        severity = "critical"
        category = "ransomware"
    strings:
        $s1 = "CryptoLocker" ascii wide
        $s2 = "Your personal files are encrypted" ascii wide
        $s3 = "private key" ascii wide
    condition:
        2 of them
}

//============================================================================
// TROJAN DETECTION
// ============================================================================

rule Trojan_Generic_Downloader {
    meta:
        description = "Detects generic trojan downloader patterns"
        author = "Zer0"
        severity = "high"
        category = "trojan"
    strings:
        $url1 = "http://" ascii wide
        $url2 = "https://" ascii wide
        $download1 = "URLDownloadToFile" ascii wide
        $download2 = "wget" ascii wide
        $download3 = "curl" ascii wide
        $exec1 = "CreateProcess" ascii wide
        $exec2 = "ShellExecute" ascii wide
        $exec3 = "system(" ascii wide
    condition:
        (any of ($url*)) and (any of ($download*)) and (any of ($exec*))
}

rule Trojan_Keylogger {
    meta:
        description = "Detects keylogger patterns"
        author = "Zer0"
        severity = "high"
        category = "trojan"
    strings:
        $api1 = "GetAsyncKeyState" ascii wide
        $api2 = "SetWindowsHookEx" ascii wide
        $api3 = "GetKeyboardState" ascii wide
        $api4 = "keybd_event" ascii wide
        $log1 = "keylog" ascii wide nocase
        $log2 = "keystroke" ascii wide nocase
    condition:
        2 of ($api*) or any of ($log*)
}

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
        $rat3 = "remote" ascii wide nocase
    condition:
        (any of ($cmd*)) and (2 of ($net*)) and (any of ($rat*))
}

//============================================================================
// BACKDOOR DETECTION
// ============================================================================

rule Backdoor_Generic {
    meta:
        description = "Detects backdoor shell patterns"
        author = "Zer0"
        severity = "critical"
        category = "backdoor"
    strings:
        $shell1 = "reverse shell" ascii wide nocase
        $shell2 = "bind shell" ascii wide nocase
        $shell3 = "nc -e" ascii wide
        $shell4 = "netcat" ascii wide nocase
        $shell5 = "/dev/tcp/" ascii wide
        $shell6 = "bash -i" ascii wide
        $php1 = "<?php" ascii
        $php2 = "eval(" ascii
        $php3 = "base64_decode" ascii
        $php4 = "shell_exec" ascii
        $php5 = "passthru" ascii
    condition:
        any of ($shell*) or (($php1) and (2 of ($php2, $php3, $php4, $php5)))
}

rule Backdoor_Webshell {
    meta:
        description = "Detects web shell patterns"
        author = "Zer0"
        severity = "critical"
        category = "backdoor"
    strings:
        $ws1 = "c99shell" ascii wide nocase
        $ws2 = "r57shell" ascii wide nocase
        $ws3 = "WSO" ascii wide
        $ws4 = "FilesMan" ascii wide
        $ws5 = "b374k" ascii wide
        $cmd1 = "$_GET['cmd']" ascii
        $cmd2 = "$_POST['cmd']" ascii
        $cmd3 = "$_REQUEST['cmd']" ascii
    condition:
        any of ($ws*) or any of ($cmd*)
}

//============================================================================
// CRYPTOMINER DETECTION
// ============================================================================

rule Cryptominer_Generic {
    meta:
        description = "Detects cryptocurrency miner patterns"
        author = "Zer0"
        severity = "medium"
        category = "cryptominer"
    strings:
        $pool1 = "stratum+tcp://" ascii wide
        $pool2 = "stratum+ssl://" ascii wide
        $pool3 = "pool.miningpool" ascii wide
        $pool4 = "xmrigpool" ascii wide
        $coin1 = "monero" ascii wide nocase
        $coin2 = "bitcoin" ascii wide nocase
        $coin3 = "ethereum" ascii wide nocase
        $miner1 = "cryptonight" ascii wide nocase
        $miner2 = "hashrate" ascii wide nocase
        $miner3 = "mining" ascii wide nocase
    condition:
        (any of ($pool*)) or ((any of ($coin*)) and (any of ($miner*)))
}

rule Cryptominer_XMRig {
    meta:
        description = "Detects XMRig miner"
        author = "Zer0"
        severity = "medium"
        category = "cryptominer"
    strings:
        $s1 = "xmrig" ascii wide nocase
        $s2 = "RandomX" ascii wide nocase
        $s3 = "cn/r" ascii wide
        $s4 = "--donate-level" ascii wide
    condition:
        2 of them
}

//============================================================================
// EXPLOIT DETECTION
// ============================================================================

rule Exploit_Shellcode {
    meta:
        description = "Detects common shellcode patterns"
        author = "Zer0"
        severity = "critical"
        category = "exploit"
    strings:
        $nop = { 90 90 90 90 90 90 90 90 }
        $shell_x86 = { 31 c0 50 68 2f 2f 73 68 68 2f 62 69 6e 89 e3 50 53 89 e1 b0 0b cd 80 }
        $shell_x64 = { 48 31 ff 48 31 f6 48 31 d2 48 31 c0 50 48 bb 2f 62 69 6e 2f 2f 73 68 53 48 89 e7 b0 3b 0f 05 }
        $meterpreter = { fc e8 82 00 00 00 60 89 e5 31 c0 64 8b 50 30 }
    condition:
        any of them
}

rule Exploit_BufferOverflow {
    meta:
        description = "Detects buffer overflow patterns"
        author = "Zer0"
        severity = "high"
        category = "exploit"
    strings:
        $pattern1 = { 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 }
        $pattern2 = { 42 42 42 42 42 42 42 42 42 42 42 42 42 42 42 42 }
        $pattern3 = { 43 43 43 43 43 43 43 43 43 43 43 43 43 43 43 43 }
        $pattern4 = { 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 }
    condition:
        (#pattern1 > 10) or (#pattern2 > 10) or (#pattern3 > 10) or (#pattern4 > 20)
}

//============================================================================
// SUSPICIOUS SCRIPTS
// ============================================================================

rule Suspicious_PowerShell {
    meta:
        description = "Detects suspicious PowerShell script patterns"
        author = "Zer0"
        severity = "high"
        category = "script"
    strings:
        $enc1 = "-EncodedCommand" ascii wide nocase
        $enc2 = "-enc " ascii wide nocase
        $enc3 = "FromBase64String" ascii wide
        $bypass1 = "-ExecutionPolicy Bypass" ascii wide nocase
        $bypass2 = "-ep bypass" ascii wide nocase
        $bypass3 = "Set-ExecutionPolicy" ascii wide nocase
        $download1 = "DownloadString" ascii wide
        $download2 = "DownloadFile" ascii wide
        $download3 = "Invoke-WebRequest" ascii wide
        $iex1 = "IEX" ascii wide
        $iex2 = "Invoke-Expression" ascii wide
    condition:
        (any of ($enc*)) or (any of ($bypass*) and any of ($download*)) or (any of ($iex*) and any of ($download*))
}

rule Suspicious_Bash {
    meta:
        description = "Detects suspicious bash script patterns"
        author = "Zer0"
        severity = "medium"
        category = "script"
    strings:
        $shebang = "#!/bin/bash" ascii
        $curl1 = "curl" ascii
        $wget1 = "wget" ascii
        $pipe1 = "| bash" ascii
        $pipe2 = "| sh" ascii
        $rm1 = "rm -r /" ascii
        $rm2 = "rm -rf ~" ascii
        $chmod1 = "chmod 777" ascii
        $chmod2 = "chmod +x" ascii
        $cron1 = "crontab" ascii
        $cron2 = "/etc/cron" ascii
    condition:
        shebang and ((any of ($curl1, $wget1)) and (any of ($pipe1, $pipe2))) or (any of ($rm1, $rm2))
}

rule Suspicious_Python {
    meta:
        description = "Detects suspicious Python patterns"
        author = "Zer0"
        severity = "medium"
        category = "script"
    strings:
        $import1 = "import socket" ascii
        $import2 = "import subprocess" ascii
        $import3 = "import os" ascii
        $exec1 = "exec(" ascii
        $exec2 = "eval(" ascii
        $exec3 = "compile(" ascii
        $b64 = "base64.b64decode" ascii
        $shell1 = "subprocess.call" ascii
        $shell2 = "os.system" ascii
        $shell3 = "os.popen" ascii
    condition:
        (2 of ($import*)) and ((any of ($exec*)) or ($b64) or (any of ($shell*)))
}

//============================================================================
// DATA EXFILTRATION
// ============================================================================

rule DataExfil_Credentials {
    meta:
        description = "Detects credential harvesting patterns"
        author = "Zer0"
        severity = "high"
        category = "exfiltration"
    strings:
        $cred1 = "password" ascii wide nocase
        $cred2 = "credential" ascii wide nocase
        $cred3 = "login" ascii wide nocase
        $browser1 = "Chrome" ascii wide
        $browser2 = "Firefox" ascii wide
        $browser3 = "Login Data" ascii wide
        $browser4 = "cookies.sqlite" ascii wide
        $send1 = "POST" ascii wide
        $send2 = "smtp" ascii wide nocase
        $send3 = "ftp" ascii wide nocase
    condition:
        (2 of ($cred*)) and (any of ($browser*)) and (any of ($send*))
}

rule DataExfil_Documents {
    meta:
        description = "Detects document exfiltration patterns"
        author = "Zer0"
        severity = "high"
        category = "exfiltration"
    strings:
        $ext1 = ".doc" ascii wide nocase
        $ext2 = ".pdf" ascii wide nocase
        $ext3 = ".xls" ascii wide nocase
        $ext4 = ".ppt" ascii wide nocase
        $search1 = "FindFirstFile" ascii wide
        $search2 = "glob" ascii wide
        $search3 = "os.walk" ascii wide
        $upload1 = "upload" ascii wide nocase
        $upload2 = "send" ascii wide nocase
        $upload3 = "exfil" ascii wide nocase
    condition:
        (2 of ($ext*)) and (any of ($search*)) and (any of ($upload*))
}

//============================================================================
// PERSISTENCE MECHANISMS
// ============================================================================

rule Persistence_Registry {
    meta:
        description = "Detects registry persistence patterns"
        author = "Zer0"
        severity = "high"
        category = "persistence"
    strings:
        $reg1 = "HKEY_CURRENT_USER\\Software" ascii wide nocase
        $reg2 = "HKEY_LOCAL_MACHINE" ascii wide nocase
        $reg3 = "RegSetValueEx" ascii wide
        $reg4 = "RegCreateKey" ascii wide
    condition:
        (any of ($reg1, $reg2)) and (2 of ($reg3, $reg4))
}

rule Persistence_Startup {
    meta:
        description = "Detects startup folder persistence"
        author = "Zer0"
        severity = "high"
        category = "persistence"
    strings:
        $startup1 = "Startup" ascii wide
        $startup2 = "Start Menu" ascii wide
        $startup3 = ".lnk" ascii wide
        $autostart1 = "autostart" ascii wide nocase
        $autostart2 = "autorun" ascii wide nocase
        $linux1 = ".bashrc" ascii
        $linux2 = ".profile" ascii
        $linux3 = "/etc/rc.local" ascii
        $linux4 = "systemctl" ascii
    condition:
        (any of ($startup1, $startup2, $startup3)) or (any of ($autostart1, $autostart2)) or (2 of ($linux*))
}

rule Persistence_Service {
    meta:
        description = "Detects service-based persistence"
        author = "Zer0"
        severity = "high"
        category = "persistence"
    strings:
        $svc1 = "CreateService" ascii wide
        $svc2 = "OpenService" ascii wide nocase
        $svc3 = "New-Service" ascii wide
        $svc4 = "systemctl enable" ascii wide
        $svc5 = "update-rc.d" ascii wide
    condition:
        any of them
}

//============================================================================
// OBFUSCATION DETECTION
// ============================================================================

rule Obfuscation_Base64 {
    meta:
        description = "Detects heavy base64 obfuscation"
        author = "Zer0"
        severity = "medium"
        category = "obfuscation"
    strings:
        $b64_long = /[A-Za-z0-9+\/]{100,}={0,2}/
        $decode1 = "base64" ascii wide nocase
        $decode2 = "atob" ascii wide
        $decode3 = "b64decode" ascii wide
    condition:
        $b64_long and any of ($decode*)
}

rule Obfuscation_XOR {
    meta:
        description = "Detects XOR obfuscation patterns"
        author = "Zer0"
        severity = "medium"
        category = "obfuscation"
    strings:
        $xor1 = "xor" ascii wide nocase
        $xor2 = "^=" ascii wide
        $loop = /for.*\^/
    condition:
        (#xor1 > 5) or (#xor2 > 10) or $loop
}

//============================================================================
// LINUX SPECIFIC
// ============================================================================

rule Linux_Rootkit {
    meta:
        description = "Detects Linux rootkit patterns"
        author = "Zer0"
        severity = "critical"
        category = "rootkit"
    strings:
        $lkm1 = "init_module" ascii
        $lkm2 = "cleanup_module" ascii
        $hide1 = "hide_pid" ascii
        $hide2 = "hide_file" ascii
        $hook1 = "sys_call_table" ascii
        $hook2 = "kallsyms" ascii
        $proc1 = "/proc/self" ascii
        $proc2 = "LD_PRELOAD" ascii
    condition:
        (all of ($lkm*)) or (any of ($hide*)) or (all of ($hook*)) or ($proc2)
}

rule Linux_PrivEsc {
    meta:
        description = "Detects Linux privilege escalation attempts"
        author = "Zer0"
        severity = "high"
        category = "privesc"
    strings:
        $suid1 = "chmod u+s" ascii
        $suid2 = "chmod 4755" ascii
        $sudo1 = "sudo -l" ascii
        $sudo2 = "/etc/sudoers" ascii
        $case1 = "case" ascii
        $case2 = "escalate" ascii
        $exploit1 = "dirty_cow" ascii nocase
        $exploit2 = "dirtycow" ascii nocase
    condition:
        any of ($suid*) or (all of ($sudo*)) or (all of ($case*)) or any of ($exploit*)
}

//============================================================================
// PACKED/ENCRYPTED EXECUTABLES
// ============================================================================

rule Packed_UPX {
    meta:
        description = "Detects UPX packed executables"
        author = "Zer0"
        severity = "low"
        category = "packer"
    strings:
        $upx1 = "UPX!" ascii
        $upx2 = "UPX0" ascii
        $upx3 = "UPX1" ascii
    condition:
        any of them
}

rule Packed_Generic {
    meta:
        description = "Detects generic packer signatures"
        author = "Zer0"
        severity = "medium"
        category = "packer"
    strings:
        $themida = "Themida" ascii wide
        $vmprotect = "VMProtect" ascii wide
        $aspack = "ASPack" ascii wide
        $pecompact = "PECompact" ascii wide
    condition:
        any of them
}
