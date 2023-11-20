# Akmal's Thesis Guide
## General Information
### Thesis Title:

`Design and Implementation of CSI Report to Support Link Adaptation in OSC O-DU MAC Scheduler`

### Forked Branch and Commit Number:
> Branch Name: sch_slice_based <br>
> Commit Hash: 2adcaeb249e4b89a87ef1221dc9060400294fe62

### Detailed Background
To access the complete documentation, please follow [this link]().

## Cloning the Repository
```bash
git clone https://github.com/NTUST-BMW-Lab/MIMO-Scheduler-on-Sch-Slice-Based.git
```

## Branch Description

| Branch Name                    | Status     | Description                                                                                                |
| ------------------------------ | ---------- | ---------------------------------------------------------------------------------------------------------- |
| master                         | Inactive   | Master Branch From OSC gerrit (only updated until around March 2023)                                       |
| CSI-extration                  | Inactive   | Jojo's Branch to extract CSI                                                                               |
| CU-DU-Config-CSI-RS            | Inactive   | Akmal's Branch to Add config related to CSI-RS from CU to DU                                               |
| CU-DU-Config-Try-New-Codec     | Inactive   | Akmal's Branch to Test new ASN.1 codec from Nokia Repository                                               |
| DU-CU-Config-CSI-RS            | Inactive   | Akmal's Branch to Add config related to CSI-RS from DU to CU                                               |
| MCS-throughput-testing         | Inactive   | This branch is used as initial proof of concept by changing MCS value                                      |
| O1-Enabled                     | Inactive   | This branch is used to run O-DU with O1 Enabled. Merged to branch o1-perf-metrics                          |
| UCI                            | Inactive   | Jojo's Branch to Test UCI.indication. Diverted from UCI-indication branch                                  |
| UCI-indication                 | Inactive   | Jojo's Branch to Test UCI.indication                                                                       |
| add_csi_data_structure         | **Active** | This branch is used exclusively to modify data structure. All other branches are referring to this branch. |
| codebook-config-data-structure | Inactive   | This branch is an attempt to add codebook config data structure. Abandoned.                                |
| develop-Akmal                  | **Active** | Akmal's main branch for development                                                                        |
| develop-Jojo                   | Inactive   | Jojo's main branch for development. Abandoned. Jojo created new repository for himself                     |
| dl-csi-rs-scheduling           | Inactive   | Branch for developing downlink csi-rs scheduling. Development finished. Branched to another branches       |
| o1-perf-metrics                | **Active** | Run O-DU with O1 enabled and send performance metrics to SMO through O1 VES                                |
| sch_slice_based                | Inactive   | Other branches are originated from this branch. Not actively developed since this is the original branch   |
| send-dummy-cqi-data            | Inactive   | This branch is used to test dummy CQI data                                                                 |
| send-dummy-cqi-data-testing    | Inactive   | Also used to test dummy CQI data with new MCS table                                                        |
| ul-tti-req-triggering          | Inactive   | Originally used to separate the development of ul tti req. Abandoned                                       |

Notes:
> Active: Possible updated in the future <br>
> Inactive: Not maintained anymore


## System Requirements
> Reference: https://docs.o-ran-sc.org/projects/o-ran-sc-o-du-l2/en/latest/installation-guide.html

### Hardware
| Hardware | Requirement |
| -------- | ----------- |
| CPU      | 4 cores     |
| RAM      | 8GB         |
| Disk     | 500GB       |
| NIC      | 1           |

### Software
This thesis is developed in Ubuntu 18.04 system. However, It can also be run in the newer version of Ubuntu with some tweaking (not explained in this guide)

> OS: Ubuntu 18.04 <br>
> Kernel Version: 4.15.0-213-generic

## Software Pre-requisite to Run O-DU
> Reference: https://docs.o-ran-sc.org/projects/o-ran-sc-o-du-l2/en/latest/installation-guide.html

1. Linux 32-bit/64-bit machine
2. Install GCC
    - On Ubuntu:
    ```bash
    sudo apt-get install -y build-essential
    ```
    - On CentOS:
    ```bash
    sudo yum group mark install -y "Development Tools"
    ```
    - **Notes**: Make sure GCC version is 4.6.3 or above by executing `gcc -version` command
3. Install LKSCTP
    - On Ubuntu:
    ```bash
    sudo apt-get install -y libsctp-dev
    ```
    - On CentOS:
    ```bash
    sudo yum install -y lksctp-tools-devel
    ```
4. Install PCAP
    - On Ubuntu:
    ```bash
    sudo apt-get install -y libpcap-dev
    ```
    - On CentOS:
    ```bash
    sudo yum install -y libpcap-devel
    ```

## Pre-requisite for O1 Interface (Required for running O-DU with O1 Enabled)
**IMPORTANT REFERENCE**
> [Heidi's Note on running O-DU High with O1 Enabled](https://hackmd.io/tmTNEE7ET4S8mt3G6wCJzA?view#2-Set-up-Netconf-Server) <br>
> [Akmal's Note on running O-DU High with O1 Enabled](https://hackmd.io/@akmalns/BJeeHprp2)

**IMPORTANT NOTE**
> We suggest to use `o1-perf-metrics` branch to test with O1 Enabled

### 1. Setup Netconf Server
1. Make sure you already follow [Software Pre-requisite to Run O-DU](#software-pre-requisite-to-run-o-du)
2. Create a new netconf user
    ```bash
    cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/build/scripts
    sudo ./add_netconf_user.sh
    ```
3. Install Netconf Libraries
    ```bash
    cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/build/scripts
    sudo ./install_lib_O1.sh -c
    ```

### 2. Update the configuration according to system setup
```bash
1. Open the startup_config.xml and edit the desired IP and Port for CU, DU and RIC.
2. Open the nacm_config.xml and edit the desired user name to provide the access to that user
3. Open the netconf_server_ipv6.xml and edit the desired netconf server configuration
4. Open the oamVesConfig.json and edit the details of OAM VES collector.
5. Open the smoVesConfig.json and edit the details of SMO VES collector
6. Open the netconfConfig.json and edit the details of netopeer server
```

**Setting used in this thesis:**
1. startup_config.xml
    - Assign IP for your O-DU, O-CU and RIC
    - In this thesis, we use the default value as follows
      - O-DU: 192.168.130.81, port:38472
      - O-CU(CU Stub): 192.168.130.82, port:38472
      - RIC(RIC Stub): 192.168.130.80, port:36422
2. nacm_config.xml
    - No change
    - We use default value
3. netconf_server_ipv6.xml
    - Change the IP value to the loopback IP address (since the netconf server is in the same machine with O-DU)
    - The loopback IP is: 127.0.0.1
4. oamVesConfig.json
    - Change the OAM VES collector IP to the **SMO IP**
    - The **SMO IP** in our lab is 192.168.8.229
    - The **OAM VES Collector Port** is 30417
5. smoVesConfig.json
    - Change the SMO VES collector IP to the **SMO IP**
    - The **SMO IP** in our lab is 192.168.8.229
    - The **SMO VES COllector Port** is 30205
6. netconfConfig.json
    - Check your NIC MAC Address and IP using `ifconfig` command
    - In ubuntu, your MAC Address usually shown next to `ether`
    - In ubuntu, your IP address usually shown nex to `inet` (IPv4)
    - In my case, the VM MAC Address is `fa:16:3e:e2:81:fe` whereas the IPv4 Address is `10.0.10.30`

### 3. Install the Yang Modules and Load Initial Configuration:
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/build/scripts
sudo ./load_yang.sh
```

### 4. Enable standard defined VES format
- Open the file in the following directory:
    > \<O-DU High Directory\>/MIMO-Scheduler-on-Sch-Slice-Based/src/o1/ves
- Open `VesUtils.h` file
- Find the preprocessor command `#define StdDef` and **uncomment** it

### 5. Start Netopeer2-server
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/build/scripts
sudo ./netopeer-server.sh start
```

## Build O-DU with **O1 Enabled**
> Reference: https://docs.o-ran-sc.org/projects/o-ran-sc-o-du-l2/en/latest/installation-guide.html

**IMPORTANT NOTE**
> We suggest to use `o1-perf-metrics` branch to test with O1 Enabled

### 1. Follow [Pre-Requisite for O1 Interface](#pre-requisite-for-o1-interface-required-for-running-o-du-with-o1-enabled)

### 2. Clean and Build O-DU
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/build/odu
make clean_odu odu MACHINE=BIT64 MODE=FDD O1_ENABLE=YES
```

### 3. Clean and Build CU Stub
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/build/odu
make clean_cu cu_stub MACHINE=BIT64 MODE=FDD O1_ENABLE=YES
```

### 4. Clean and Build RIC Stub
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/build/odu
make clean_ric ric_stub NODE=TEST_STUB MACHINE=BIT64 MODE=FDD O1_ENABLE=YES
```

### IMPORTANT NOTES
If you encounter some build error after following [Pre-requisite](#pre-requisite-for-o1-interface-required-for-running-o-du-with-o1-enabled) and [Build O-DU](#build-o-du-with-o1-enabled), please check the following resource:
- [Issue 1](https://hackmd.io/kqHD0l7WQ3OnPtWiabsKcA?view#Error-Fix)
- [Issue 2: Build CU Stub](https://hackmd.io/@akmalns/BkQaUMQph#b-Build-CU-Stub)

## Run O-DU with O1 Enabled
**Note**
> CU Stub, RIC Stub, O-DU and netopeer-cli should be run on different terminals
### 1. Assign IP to the O-DU, CU Stub and RIC Stub
```bash
sudo ifconfig <interface-name>:ODU "192.168.130.81"
sudo ifconfig <interface-name>:CU_STUB "192.168.130.82"
sudo ifconfig <interface-name>:RIC_STUB "192.168.130.80"
```

### 2. Run CU Stub
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/bin/cu_stub
sudo ./cu_stub
```

### 3. Run RIC Stub
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/bin/ric_stub
sudo ./ric_stub
```

### 4. Run O-DU
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/bin/odu
sudo ./odu
```

- Wait until O-DU successfully sent registration to SMO before proceeding to the next step
- It usually indicated with the following line:
    ```bash
    O1 PnfRegistrationThread : Sending PNF registration. Attempt 0
    Successfully send event
    O1 PnfRegistrationThread : Sending PNF registration. Attempt 1
    Successfully send event
    ``````

### 5. Push cell configuration using Netopeer-CLI
> Reference: [Heidi's Note](https://hackmd.io/tmTNEE7ET4S8mt3G6wCJzA?view#Push-cell-and-slice-configuration-over-O1-using-netopeer-cli)

- Access netopeer2-cli
    ```bash
    ## Go to folder containing the config files
    cd <O-DU High Directory>/l2/build/config
    ## Start netopeer2-cli
    netopeer2-cli
    ```
- Connect to netconf server via netopeer2-cli
    ```bash
    > connect --login netconf
    ```
    The default password should be **netconf!**
- Push cell configuration via netopeer2-cli
    ```bash
    > edit-config --target candidate --config=cellConfig.xml
    OK
    > commit
    OK
    ```
    After pushing the cell configuration, **O-DU terminal should continue running**

### Notes
- If you want to run another test after finish running O-DU, you need to change the value of `id` in `cellConfig.xml` and `id` in `rrmPolicy.xml`
- For example, in the first running, we use `gnb-1` and `rrm-1` as the id
- On the next running session, before push cell configuration via netopeer2-cli, modify the id into `gnb-2` and `rrm-2`
- After restarting VM/Machine, you can reset the id from `gnb-1` and `rrm-1`

## Build and Run O-DU **without** O1 Enabled
> Reference: https://docs.o-ran-sc.org/projects/o-ran-sc-o-du-l2/en/latest/installation-guide.html
### 1. Clean and Build O-DU
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/build/odu
make clean_odu odu MACHINE=BIT64 MODE=FDD
```

### 2. Clean and Build CU Stub
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/build/odu
make clean_cu cu_stub NODE=TEST_STUB MACHINE=BIT64 MODE=FDD
```

### 3. Clean and Build RIC Stub
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/build/odu
make clean_ric ric_stub NODE=TEST_STUB MACHINE=BIT64 MODE=FDD
```

### 4. Run CU Stub
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/bin/cu_stub
sudo ./cu_stub
```

### 5. Run RIC Stub
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/bin/ric_stub
sudo ./ric_stub
```

### 6. Run O-DU
```bash
cd <O-DU High Directory>/MIMO-Scheduler-on-Sch-Slice-Based/bin/odu
sudo ./odu
```