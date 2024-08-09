# DriverJack

## Introduction

**DriverJack** is a tool designed to load a vulnerable driver by abusing lesser-known NTFS techniques. These method bypass the registration of a Driver Service on the system by hijacking an existing service, and also spoof the image path presented in the Driver Load event. To further masquerade the presence of a vulnerable driver, the attack also abuses an Emulated Filesystem Read-Only bypass to swap the content of a driver file on a mounted ISO before loading it.

## Emulated Filesystem Read-Only Bypass

DriverJack abuses the possibility of remapping files mounted on emulated filesystems to RW pages to overwrite their contents. This RO bypass is implemented in **IoCdfsLib**.

## Attack Overview

Once the ISO is mounted, the attack proceeds by selecting a service driver that can be started or stopped, or one that can be triggered, requiring administrative privileges unless misconfigured. 

### Key Attack Phases

1. **ISO Mounting and Driver Selection**
   - The attack begins with mounting the ISO as a filesystem.
   - The attacker selects a service driver that can be manipulated, focusing on those that can be started or restarted without immediate detection.

2. **Hijacking the Driver Path**
   - The core of the attack involves hijacking the driver path. The methods used include:
     - **Direct Reparse Point Abuse**
     - **DosDevice Global Symlink Abuse**
     - **Drive Mountpoint Swap**

### Attack Techniques

#### 1. Direct Reparse Point Abuse

This technique exploits the ability of an installer to access the `C:\Windows\System32\drivers` directory directly, allowing a malicious symbolic link to be placed there. The symbolic link is processed by the OS with precedence, leading to the malicious driver being loaded when the service is restarted.

**Key Steps:**
- The `NtLoadDriver` function normalizes the NT Path of the symbolic link.
- When the service is restarted, the malicious driver is loaded. However, the Load Driver event will show the real path of the driver image being loaded, pointing to the ISO mountpoint.

#### 2. NT Symlink Abuse

Developed in collaboration with jonasLyk of the Secret Club hacker collective, this method involves redirecting the `\Device\BootDevice` NT symbolic link, part of the path from which a driver binary is loaded. This allows for the hiding of a rootkit within the system.

**Steps:**
- Gain SYSTEM privileges.
- Backup the `BootDevice` symlink target.
- Tamper with the `BootDevice` symlink to point to the mounted ISO.
- Start/Restart the service.
- Restore the `BootDevice` symlink target.

This method was inspired by techniques used in the unDefender project to disable the Windows Defender service and driver. The Load Driver event will still show the real path of the driver being loaded, pointing to the ISO mountpoint.

#### 3. Mount Point Swapping

Although widely known, this technique is rarely used due to the potential for system instability. It involves temporarily changing the drive letter assigned to the `BootPartition`, tricking the driver load process to access a different drive.
When combined with NT Symlink Abuse, explained before, this technique can completely masquerade the path of the driver being loaded, bypassing detection by SysMon and other monitoring tools.

## Conclusion

**DriverJack** demonstrates another, non-conventional way for vulnerable driver-loading that leverages CDFS emulated filesystems and lesser-known NTFS symbolic link properties. 

## Thanks

* Jonas Lyk for all the help and the brainstorming sessions. 

## Additional References

* unDefender by APT Tortellini

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
