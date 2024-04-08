/*!
 * @file ShieldCommunication.cpp
 * @brief Software for IO-Link shield usage, main function
 * @copyright 2022 Balluff GmbH
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *	    http://www.apache.org/licenses/LICENSE-2.0
 *
 *	 Unless required by applicable law or agreed to in writing, software
 *	 distributed under the License is distributed on an "AS IS" BASIS,
 *	 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	 See the License for the specific language governing permissions and
 *	 limitations under the License.
 * @author See AUTHORS file
 * @since 11.08.2022
 */

//!**** Header-Files ***********************************************************
#include "ShieldCommunication.h"
#include <mosquitto.h>
#include <stdio.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iostream>
#include <string>
#include <chrono>
#include <spdlog/fmt/fmt.h>

using json = nlohmann::json;

//!**** Definitions ***********************************************************
#define SWAP_INT32(x) (((x) >> 24) | (((x)&0x00FF0000) >> 8) | (((x)&0x0000FF00) << 8) | ((x) << 24));

//!**** Implementation *********************************************************

//!*******************************************************************************
//!  function :    ShieldCommunication
//!*******************************************************************************
//!  \brief        Constructor of ShieldCommunication
//!
//!  \type         local
//!
//!  \param[in]    bool extended_board
//!
//!  \return       void
//!
//!*******************************************************************************

ShieldCommunication::ShieldCommunication(bool extended_board)
{
    Communication_startup(extended_board);
}

//!*******************************************************************************
//!  function :    ~ShieldCommunication
//!*******************************************************************************
//!  \brief        Destructor of ShieldCommunication
//!
//!  \type         local
//!
//!  \param[in]    void
//!
//!  \return       void
//!
//!*******************************************************************************

ShieldCommunication::~ShieldCommunication()
{
    Communication_shutdown();
}

//!*******************************************************************************
//!  function :    Communication_startup
//!*******************************************************************************
//!  \brief        The setup function is called in constructor (once at startup)
//!
//!  \type         local
//!
//!  \param[in]    bool extended_board
//!
//!  \return       void
//!
//!*******************************************************************************

void ShieldCommunication::Communication_startup(bool extended_board)
{
    // Create hardware setup
    hardware = HardwareRaspberry();
    hardware.begin();

    // Create IODD manager
    //instance = IoddManager();
    service = IoddService();

    // Create drivers
    max14819::Max14819 *pDriver01 = new max14819::Max14819(max14819::DRIVER01, &hardware);
    max14819::Max14819 *pDriver23 = new max14819::Max14819(max14819::DRIVER23, &hardware);
    // Create ports
    ports.push_back(IOLMasterPortMax14819(pDriver01, max14819::PORT0PORT));
    ports.push_back(IOLMasterPortMax14819(pDriver01, max14819::PORT1PORT));
    if (extended_board)
    {
        ports.push_back(IOLMasterPortMax14819(pDriver23, max14819::PORT2PORT));
        ports.push_back(IOLMasterPortMax14819(pDriver23, max14819::PORT3PORT));
    }

    // Start IO-Link communication
    for (auto &nr : ports)
    {
        nr.begin();
    }

    // Start MQTT init
    mosquitto_lib_init();

    int rc;
    mosq = mosquitto_new("pi", true, NULL);
    rc = mosquitto_connect(mosq, brokerIP.c_str(), 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        printf("Error connecting to broker: %s\n", mosquitto_strerror(rc));
        mosquitto_destroy(mosq);
        return;
    }
}

//!*******************************************************************************
//!  function :    Communication_startup
//!*******************************************************************************
//!  \brief        The setup function is called in destructor (once at shutdown)
//!
//!  \type         local
//!
//!  \param[in]    void
//!
//!  \return       void
//!
//!*******************************************************************************

void ShieldCommunication::Communication_shutdown()
{
    // Stop IO-Link communication
    for (auto &nr : ports)
    {
        nr.end();
    }
    char buf[] = "Stop IO-Link communication";
    hardware.Serial_Write(buf);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);

    mosquitto_lib_cleanup();
}

//!*******************************************************************************
//!  function :    signalHandler
//!*******************************************************************************
//!  \brief
//!
//!  \type         local
//!
//!  \param[in]    int signum
//!
//!  \return       void
//!
//!*******************************************************************************

void ShieldCommunication::signalHandler(int signum)
{
    char buf[] = "Interrupt signal received";
    hardware.Serial_Write(buf);
    // cleanup and close up stuff here
    // terminate program
    Communication_shutdown();
    exit(signum);
}

//!*******************************************************************************
//!  function :    ISDU_Write
//!*******************************************************************************
//!  \brief        Function to proof if a device is connected and after that
//!                writing ISDU-Data
//!
//!  \type         local
//!
//!  \param[in]    uint8_t port_nr
//!                uint16_t index
//!                uint8_t subIndex
//!                vector<uint8_t> oData
//!
//!  \return       void
//!
//!*******************************************************************************

void ShieldCommunication::ISDU_Write(uint8_t port_nr, uint16_t index, uint8_t subIndex, vector<uint8_t> oData)
{
    uint8_t retVal = SUCCESS;
    // check if Port is connected
    int OnRequestData = 0;
    int ProcessDataIn = 0;
    int ProcessDataOut = 0;

    OnRequestData = get<0>(ports.at(port_nr).getLengthParameter());
    ProcessDataIn = get<1>(ports.at(port_nr).getLengthParameter());
    ProcessDataOut = get<2>(ports.at(port_nr).getLengthParameter());

    if (OnRequestData || ProcessDataIn || ProcessDataOut) // if Device Connected -> write Data to device
    {
        if (port_nr == 0 || 1)
        {
            max1Mutex.lock();
        }
        else
        {
            max2Mutex.lock();
        }
        retVal = ports.at(port_nr).writeISDU(oData.size(), oData, index, subIndex);
        if (port_nr == 0 || 1)
        {
            max1Mutex.unlock();
        }
        else
        {
            max2Mutex.unlock();
        }
    }
    else
    {
        retVal = ERROR;
        cout << "ERROR - No Device connected" << endl;
    }
    return;
}

//!*******************************************************************************
//!  function :    ISDU_Read
//!*******************************************************************************
//!  \brief        Function to proof if a device is connected and after that
//!                reading ISDU-Data
//!
//!  \type         local
//!
//!  \param[in]    uint8_t port_nr
//!                uint16_t index
//!                uint8_t subIndex
//!                vector<uint8_t> &oData (as reference)
//!
//!  \return       void
//!
//!*******************************************************************************

void ShieldCommunication::ISDU_Read(uint8_t port_nr, uint16_t index, uint8_t subIndex, vector<uint8_t> &Data)
{
    uint8_t retVal = SUCCESS;
    vector<uint8_t> oData;

    // check if Port is connected
    int OnRequestData = 0;
    int ProcessDataIn = 0;
    int ProcessDataOut = 0;

    OnRequestData = get<0>(ports.at(port_nr).getLengthParameter());
    ProcessDataIn = get<1>(ports.at(port_nr).getLengthParameter());
    ProcessDataOut = get<2>(ports.at(port_nr).getLengthParameter());

    if (OnRequestData || ProcessDataIn || ProcessDataOut) // if Device Connected -> write Data to device
    {
        // hardware.wait_for(500);
        if (port_nr == 0 || 1)
        {
            max1Mutex.lock();
        }
        else
        {
            max2Mutex.lock();
        }
        // read ISDU
        retVal = ports.at(port_nr).readISDU(oData, index, subIndex);
        if (port_nr == 0 || 1)
        {
            max1Mutex.unlock();
        }
        else
        {
            max2Mutex.unlock();
        }
    }
    else
    {
        retVal = ERROR;
        cout << "ERROR - No Device connected" << endl;
    }
    Data = oData;
    /*for(int i=0;i<Data.size();i++)
    {
        cout<<"Wert: "<<int(Data[i])<<endl;
    }*/
    return;
}

//!*******************************************************************************
//!  function :    send_all_PD
//!*******************************************************************************
//!  \brief        Function to send all Process Data per MQTT to MQTT-Broker
//!
//!  \type         local
//!
//!  \param[in]    void
//!
//!  \return       void
//!
//!*******************************************************************************

void ShieldCommunication::send_all_PD()
{
    // TIMESTAMP
    timeval curTime;
    gettimeofday(&curTime, NULL);
    int milli = curTime.tv_usec / 1000;

    char buffer[80];
    strftime(buffer, 80, "%Y-%m-%dT%H:%M:%S", localtime(&curTime.tv_sec));

    char currentTime[84] = "";
    sprintf(currentTime, "%s:%03d", buffer, milli);

    string TOPIC_ORIGINATOR_ID = "Shield";
    string TOPIC_PORT = "Port";
    string TOPIC_DATA_SELECTOR_EVENT = "pd";
    nlohmann::json jsonobject;
    string jsonstring;
    char *pointer;
    int OnRequestData = 0;
    int ProcessDataIn = 0;
    int ProcessDataOut = 0;
    int port_nr = 0;
    for (auto &nr : ports)
    {
        OnRequestData = get<0>(nr.getLengthParameter());
        ProcessDataIn = get<1>(nr.getLengthParameter());
        ProcessDataOut = get<2>(nr.getLengthParameter());

        if (OnRequestData || ProcessDataIn || ProcessDataOut) // if Device Connected
        {
            // TOPIC
            std::string topic_str = fmt::format("{}/{}{}/{}", TOPIC_ORIGINATOR_ID, TOPIC_PORT, std::to_string(port_nr), TOPIC_DATA_SELECTOR_EVENT);
            // JSON
            jsonobject = nr.get_PDclass()->interpretProcessData(service);
            jsonobject["ts"] = currentTime;
            jsonstring = jsonobject.dump();
            std::cout << "Port Nr: " << port_nr << std::endl;
            std::cout << jsonstring << std::endl;
            pointer = &jsonstring[0];
            int qos = 0; // QoS level
            int retVal = mosquitto_publish(mosq, NULL, topic_str.c_str(), jsonstring.size(), pointer, qos, false);
        }
        port_nr++;
    }

    return;
}

//!*******************************************************************************
//!  function :    get_PD_portx
//!*******************************************************************************
//!  \brief        ???
//!
//!  \type         local
//!
//!  \param[in]    void
//!
//!  \return       vector<uint8_t> pData
//!
//!*******************************************************************************

vector<uint8_t> ShieldCommunication::get_PD_portx(string port)
{
    return pData;
}

//!*******************************************************************************
//!  function :    timeSinceEpochMillisec
//!*******************************************************************************
//!  \brief        returns time for a counter
//!
//!  \type         local
//!
//!  \param[in]    void
//!
//!  \return       timeSinceEpoch in ms
//!
//!*********************************************************

int ShieldCommunication::timeSinceEpochMillisec()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

//!*******************************************************************************
//!  function :    Read_port
//!*******************************************************************************
//!  \brief        Function to proof if a device is connected and after that
//!                reading Process-Data and store it in the right variable in PDclass
//!
//!  \type         local
//!
//!  \param[in]    uint8_t port_nr
//!
//!  \return       void
//!
//!*********************************************************

void ShieldCommunication::Read_port(uint8_t port_nr)
{
    uint8_t retVal = SUCCESS;

    vector<uint8_t> pData;

    while (true)
    {
        if (ports.at(port_nr).get_DeviceConnection() == 0)
        { // if Device Connected
            if (port_nr == 0 || port_nr == 1)
            {
                max1Mutex.lock();
            }
            else
            {
                max2Mutex.lock();
            }
            ports.at(port_nr).readPD(pData);
            retVal = ports.at(port_nr).readErrorRegister();
            ports.at(port_nr).get_PDclass()->write_pd_storage(pData);
            if (port_nr == 0 || port_nr == 1)
            {
                max1Mutex.unlock();
            }
            else
            {
                max2Mutex.unlock();
            }
            if (pData.size() > 0)
            {
                break; // Exit the loop if data was successfully read
            }
        }
        else
        {
            retVal = ERROR;
            // cout<<"ERROR - No Device connected"<<endl;
            break; // Exit the loop if no device is connected
        }
    }
    return;
}

//!*******************************************************************************
//!  function :    PD_all_ports
//!*******************************************************************************
//!  \brief        Function to read PD and write PD of all ports
//!
//!  \type         local
//!
//!  \param[in]    void
//!
//!  \return       void
//!
//!*********************************************************

void ShieldCommunication::PD_all_ports()
{
    string currentTime;
    typedef std::chrono::high_resolution_clock Time;
    typedef std::chrono::milliseconds ms;
    typedef std::chrono::duration<float> fsec;
    string TOPIC_ORIGINATOR_ID = "Shield";
    string TOPIC_PORT = "Port";
    string TOPIC_DATA_SELECTOR_EVENT = "pd";
    nlohmann::json jsonobject;
    nlohmann::json tmpobject;
    string jsonstring;
    string tmpstring;
    char *pointer;
    int OnRequestData = 0;
    int ProcessDataIn = 0;
    int ProcessDataOut = 0;
    int port_nr = 0;
    while (1)
    {
        currentTime = getCurrentTimeStamp();
        auto t0 = Time::now();
        for (auto &nr : ports)
        {
            Read_port(port_nr);
            hardware.wait_for(1);
            Write_Port(port_nr);
            OnRequestData = get<0>(nr.getLengthParameter());
            ProcessDataIn = get<1>(nr.getLengthParameter());
            ProcessDataOut = get<2>(nr.getLengthParameter());
            if (OnRequestData || ProcessDataIn || ProcessDataOut) // if Device Connected
            {
                // TOPIC
                std::string topic_str = fmt::format("{}/{}{}/{}", TOPIC_ORIGINATOR_ID, TOPIC_PORT, std::to_string(port_nr), TOPIC_DATA_SELECTOR_EVENT);
                // JSON
                jsonobject = nr.get_PDclass()->interpretProcessData(service);
                jsonobject["ts"] = currentTime;
                jsonstring = jsonobject.dump();
               /* tmpobject = nr.get_PDclass()->interpretProcessData(instance);
                tmpobject["ts"] = currentTime;
                tmpstring = tmpobject.dump();*/
              cout << "---------------------------------------------------------------------------------------------------------------" << endl;
               cout << "interpreted ProcessData new: " << jsonstring << endl;
             //  cout << "interpreted ProcessData old: " << tmpstring << endl;
                pointer = &jsonstring[0];
                int qos = 0; // QoS level
                int retVal = mosquitto_publish(mosq, NULL, topic_str.c_str(), jsonstring.size(), pointer, qos, false);
            }
            port_nr++;
        }
        port_nr = 0;
        auto t1 = Time::now();
        fsec fs = t1 - t0;
        ms d = std::chrono::duration_cast<ms>(fs);
        if (cycleTime - d.count() >= 0)
            hardware.wait_for(uint32_t(cycleTime - uint32_t(d.count())));
    }
    return;
}

//!*******************************************************************************
//!  function :    Write
//!*******************************************************************************
//!  \brief        Function to proof if a device is connected and after that
//!                writing Process-Data of the stored variable in
//!
//!  \type         local
//!
//!  \param[in]    void
//!
//!  \return       void
//!
//!*********************************************************

void ShieldCommunication::Write_Port(uint8_t port_nr)
{
    uint8_t retVal = SUCCESS;
    // check if Port is connected
    int OnRequestData = 0;
    int ProcessDataIn = 0;
    int ProcessDataOut = 0;

    OnRequestData = get<0>(ports.at(port_nr).getLengthParameter());
    ProcessDataIn = get<1>(ports.at(port_nr).getLengthParameter());
    ProcessDataOut = get<2>(ports.at(port_nr).getLengthParameter());

    if (ports.at(port_nr).get_DeviceConnection() == 0) // if Device Connected -> write Data to device
    {
        if (ProcessDataOut == 0)
            return;
        uint8_t sizeAnswer = 2; // MC + CKS
        vector<uint8_t> Datavector = ports.at(port_nr).get_PDclass()->get_procDataOut();
        if (Datavector.size() != ProcessDataOut)
        {
            Datavector.clear();
            for (int i = 0; i < ProcessDataOut; i++)
            {
                Datavector.push_back(0);
            }
        }
        Datavector.push_back(IOL::MC::PDOUT_VALID);
        for (int i = 1; i < OnRequestData; i++)
        {
            Datavector.push_back(0);
        }
        uint8_t sizeData = Datavector.size();

        if (ProcessDataOut + OnRequestData != int(sizeData)) // wrong PDout Length!
        {
            cout << "ERROR - Wrong PDout Length" << endl;
            return;
        }

        if (port_nr == 0 || 1)
        {
            max1Mutex.lock();
        }
        else
        {
            max2Mutex.lock();
        }
        uint8_t Data[Datavector.size()];
        for (int i = 0; i < Datavector.size(); i++)
        {
            Data[i] = Datavector[i];
        }
        uint8_t *pData = &Data[0];
        retVal = ports.at(port_nr).writePD(sizeData, pData, sizeAnswer);
        if (port_nr == 0 || 1)
        {
            max1Mutex.unlock();
        }
        else
        {
            max2Mutex.unlock();
        }
    }
    else
    {
        retVal = ERROR;
        // cout<<"ERROR - No Device connected"<<endl;
    }
    return;
}

//!*******************************************************************************
//!  function :    Write_procDataOut
//!*******************************************************************************
//!  \brief        Function to proof if a device is connected and after that
//!                reading Process-Data and store it in the right variable in PDclass
//!
//!  \type         local
//!
//!  \param[in]    uint8_t port_nr
//!                vector<uint8_t> Data
//!
//!  \return       void
//!
//!*********************************************************

void ShieldCommunication::Write_procDataOut(uint8_t port_nr, vector<uint8_t> Data)
{
    cout<<"Write_procDataOut"<<endl;
    for (uint8_t i : Data) {
        cout << int(i) << endl;
    }
    ports.at(port_nr).get_PDclass()->write_procDataOut(Data);
    return;
}

//!*******************************************************************************
//!  function :    writeCycleTime
//!*******************************************************************************
//!  \brief        ???
//!
//!  \type         local
//!
//!  \param[in]    int time_in_ms
//!
//!  \return       void
//!
//!*********************************************************

void ShieldCommunication::writeCycleTime(int time_in_ms)
{
    cycleTime = time_in_ms;
    return;
}

//!*******************************************************************************
//!  function :    isDeviceConnected
//!*******************************************************************************
//!  \brief        check on all ports if a device is connected (triggered by CROW)
//!
//!  \type         local
//!
//!  \param[in]    vector<uint8_t>& portConnection
//!
//!  \return       void
//!
//!*********************************************************

void ShieldCommunication::isDeviceConnected(vector<uint8_t> &portConnection)
{
    int portNummer = 0;
    for (auto &nr : ports)
    {
        if (portNummer == 0 || 1)
        {
            max1Mutex.lock();
        }
        else
        {
            max2Mutex.lock();
        }

        nr.isDeviceConnected();

        if (portNummer == 0 || 1)
        {
            max1Mutex.unlock();
        }
        else
        {
            max2Mutex.unlock();
        }

        if (nr.get_DeviceConnection() == 0)
            portConnection.push_back(0);
        else
            portConnection.push_back(1);

        if (portNummer >= 3)
            portNummer = 0;
        else
            portNummer++;
    }
    return;
}

//!*******************************************************************************
//!  function :    getCycleTime
//!*******************************************************************************
//!  \brief        get the cycle Time to set the slider on the node red ui
//!
//!  \type         local
//!
//!  \param[in]    void
//!
//!  \return       int cycleTime
//!
//!*********************************************************

int ShieldCommunication::getCycleTime()
{
    return cycleTime;
}

void ShieldCommunication::writeIP(string newIP)
{
    brokerIP.clear();
    brokerIP = newIP;
    return;
}

std::string ShieldCommunication::getCurrentTimeStamp()
{
    // aktuelle Zeit holen
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(now);

    // Zeit in den richtigen String-Formaten umwandeln
    std::stringstream ss;
    ss << std::put_time(std::localtime(&timer), "%Y-%m-%dT%H:%M:%S") << ":" << std::setfill('0') << std::setw(3) << ms.count();

    return ss.str();
}

int main()
{

    crow::SimpleApp app;

    ShieldCommunication shield(true); // create object of shield

    // Threads
    // Start the PD thread (read/writes PD cyclic)
    thread PD_all_portsThread(&ShieldCommunication::PD_all_ports, &shield);
    PD_all_portsThread.detach();

    // CROW
    //===================================================================================================================================
    CROW_ROUTE(app, "/writeProcessData") // send the ProcessData + Port you want the data to be written
        .methods("POST"_method)([&shield](const crow::request &req)
                                {

        auto x = crow::json::load(req.body);

        if (!x) return crow::response(400);

        vector<uint8_t> pData;
        pData.clear();

        string hexStr = string(x["Data"]);
        if((hexStr.length()%2)!=0) hexStr ="0"+hexStr; //length correction (even)

        //Hilfsvariablen
        string hstr;
        int k=0;
        unsigned int hilfsvar;
        //hex in string to int
        for(int i=0; i<hexStr.size();i=i+2)
        {
            hstr.push_back(hexStr[i]);
            hstr.push_back(hexStr[i+1]);
            istringstream iss(hstr);
            iss >> hex >> hilfsvar;
            pData.push_back(uint8_t(hilfsvar));
            k++;
            hstr.clear();
        }
        //end of conversion

        shield.Write_procDataOut(uint8_t(x["Port"].i()), pData); //calling method to actually write

        std::ostringstream os;
        
        os << "Process Data was written!";

        return crow::response{os.str()}; });

    //===================================================================================================================================
    CROW_ROUTE(app, "/writeCycleTime") // add a delay in reading & writing ProcessData
        .methods("POST"_method)([&shield](const crow::request &req)
                                {

        auto x = crow::json::load(req.body);

        if (!x) return crow::response(400);

        shield.writeCycleTime( int(x["cycleTime"].i())); //calling method of writing the actual cycle time

        std::ostringstream os;
        
        os << "Cycle Time was written successfully!";

        return crow::response{os.str()}; });

    //====================================================================================================================================
    CROW_ROUTE(app, "/readCycleTime") // add a delay in reading & writing ProcessData
        .methods("GET"_method)([&shield]()
                               {

        int returnValue;

        returnValue = int(shield.getCycleTime()); //calling method of reading the actual cycle time

        std::ostringstream os;
        
        os << int(returnValue);

        return crow::response{os.str()}; });

    //===================================================================================================================================
    CROW_ROUTE(app, "/readisdu") // send request with Port Index Subindex to read out the selected ISDU Data
        .methods("POST"_method)([&shield](const crow::request &req)
                                {

            auto x = crow::json::load(req.body);
            
            if (!x) return crow::response(400);

            vector<uint8_t> oData;

            //cout<<"Port :"<<int(x["Port"].i()) << " Index: " << int(x["Index"].i()) << " Subindex: " << int(x["Subindex"].i())<<endl;

            thread readISDUThread(&ShieldCommunication::ISDU_Read, &shield, uint8_t(x["Port"].i()), uint16_t(x["Index"].i()), uint8_t(x["Subindex"].i()), ref(oData)); //starting a thread because you maybe need IOL Communication

            readISDUThread.join();  

            std::ostringstream os;

            for(int i=0;i<oData.size();i++)
            {
                stringstream stream;
                stream<<hex<<int(oData.at(i));
                os << stream.str() << " ";
                //os << int(oData.at(i)) <<" ";
            }
        crow::json::wvalue returnObject;

        returnObject["Port"]=x["Port"];
        returnObject["Data"]=os.str();
            //return crow::response{os.str()};
        return  crow::response{returnObject}; });

    //===================================================================================================================================
    CROW_ROUTE(app, "/writeisdu") // send a Port Index Subindex and Data to write it in the selected ISDU Register
        .methods("POST"_method)([&shield](const crow::request &req)
                                {

        auto x = crow::json::load(req.body);
    
        if (!x) return crow::response(400);

        vector<uint8_t> oData;

        oData.clear();

        string str = string(x["Data"]);
        if((str.length()%2)!=0) str ="0"+str; //length correction
        string hstr;
        int k=0;
        unsigned int hilfsvar;
        for(int i=0; i<str.size();i=i+2)
        {
            hstr.push_back(str[i]);
            hstr.push_back(str[i+1]);
            istringstream iss(hstr);
            iss >> hex >> hilfsvar;
            oData.push_back(uint8_t(hilfsvar));
            k++;
            hstr.clear();
        }
        /*for(int i=0;i<oData.size();i++)
        {
            cout<<"oData: " << int(oData[i])<<endl;
        }*/
        thread readISDUThread(&ShieldCommunication::ISDU_Write, &shield,  uint8_t(x["Port"].i()), uint16_t(x["Index"].i()), uint8_t(x["Subindex"].i()), oData); //starting a thread because you maybe need IOL Communication
        readISDUThread.join();

        std::ostringstream os;

        for(int i=0;i<oData.size();i++)
        {
            os << int(oData.at(i));
        }

        return crow::response{os.str()}; });

    //===================================================================================================================================

    CROW_ROUTE(app, "/checkDevices") // please use GET-methods
    ([&shield]()
     {
        vector<uint8_t> portConnection;

        thread isDeviceConnectedThread(&ShieldCommunication::isDeviceConnected,&shield, ref(portConnection)); //starting a thread because you maybe need IOL Communication
        isDeviceConnectedThread.join();

        crow::json::wvalue returnObject;
        for(int portNummer=3;portNummer>=0;portNummer=portNummer-1)
        {
            if(portConnection[portNummer]==0)
            {
                switch(portNummer)
                {
                    case 0:
                    returnObject ["Port0"] = true;
                    break;
                    case 1:
                    returnObject ["Port1"] = true;
                    break;
                    case 2:
                    returnObject ["Port2"] = true;
                    break;
                    case 3:
                    returnObject ["Port3"] = true;
                    break;
                    default:
                    break;
                }
            }
            else
            {
                switch(portNummer)
                    {
                    case 0:
                    returnObject ["Port0"] = false;
                    break;
                    case 1:
                    returnObject ["Port1"] = false;
                    break;
                    case 2:
                    returnObject ["Port2"] = false;
                    break;
                    case 3:
                    returnObject ["Port3"] = false;
                    break;
                    default:
                    break;
                    }
            }
        }
    return returnObject; });

    CROW_ROUTE(app, "/changeipforbroker") // send a Port Index Subindex and Data to write it in the selected ISDU Register
        .methods("POST"_method)([&shield](const crow::request &req)
                                {

        auto x = crow::json::load(req.body);
    
        if (!x) return crow::response(400);

        string hstr = string(x["newIP"]);

        shield.writeIP(hstr);

        std::ostringstream os;

        os << "Done!";

        return crow::response{os.str()}; });

    //======End API======

    app.port(18080).multithreaded().run();

    return 0;
}