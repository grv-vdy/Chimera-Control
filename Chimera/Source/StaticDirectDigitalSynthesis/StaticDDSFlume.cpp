// ///Archive ZZP

// #include "stdafx.h"
// #include "StaticDDSFlume.h"

// StaticDDSFlume::StaticDDSFlume(std::string portAddress, unsigned baudrate, bool safemode)
// 	: boostFlume(safemode, portAddress, baudrate)
// 	, SAFEMODE(safemode)
// 	, readComplete(true)
// {
// 	boostFlume.setReadCallback(boost::bind(&StaticDDSFlume::readCallback, this, _1));
// 	boostFlume.setErrorCallback(boost::bind(&StaticDDSFlume::errorCallback, this, _1));
// }

// std::string StaticDDSFlume::query(std::string msg)
// {
//     return std::string();
// }

// void StaticDDSFlume::write(std::string msg)
// {
// 	readRegister.clear();
// 	errorMsg.clear();
// 	readComplete = false;
// 	/*write data to serial port*/
// 	std::vector<unsigned char> byteMsg(msg.cbegin(), msg.cend());
// 	boostFlume.write(byteMsg);
// 	/*check exception after write*/
// 	if (auto e = boostFlume.lastException()) {
// 		try {
// 			boost::rethrow_exception(e);
// 		}
// 		catch (boost::system::system_error& e) {
// 			throwNested("Error seen in writing to serial port " + str(boostFlume.portID) + ". Error: " + e.what());
// 		}
// 	}
// }

// // This should be called with a expectation of reading something, e.g. after writing and that is why the reading register etc is not initialized. 
// // Otherwise it will throw
// std::string StaticDDSFlume::read()
// {
// 	if (SAFEMODE) {
// 		return std::string("static DDS is in safemode.");
// 	}
// 	std::string recv;
// 	/*read register after write*/
// 	for (auto idx : range(200)) {
// 		if (readComplete) {
// 			recv = std::string(readRegister.cbegin(), readRegister.cend());
// 			break;
// 		}
// 		Sleep(1);
// 	}
// 	/*check reading error and reading result and if reading is complete*/
// 	if (recv.empty() || !readComplete) {
// 		thrower("Reading is empty and timed out for 200ms in reading from windfreak serial port " + str(boostFlume.portID) + ".");
// 	}
// 	if (!errorMsg.empty()) {
// 		thrower("Nothing feeded back from static DDS, something might be wrong with it." + recv + "\r\nError message: " + errorMsg);
// 	}
// 	recv.erase(std::remove(recv.begin(), recv.end(), '\n'), recv.end());
// 	return recv;
// }

// void StaticDDSFlume::resetConnection()
// {
// 	boostFlume.disconnect();
// 	Sleep(10);
// 	boostFlume.reconnect();
// }

// void StaticDDSFlume::readCallback(int byte)
// {
// 	if (byte < 0 || byte >255) {
// 		thrower("Byte value readed needs to be in range 0-255.");
// 	}
// 	readRegister.push_back(byte);
// 	if (byte == '\n') {
// 		readComplete = true;
// 	}
// }

// void StaticDDSFlume::errorCallback(std::string error)
// {
// 	errorMsg = error;
// }

///



#include "stdafx.h"
#include "StaticDDSFlume.h"

#include "stdafx.h"
#include "StaticDDSFlume.h"

StaticDDSFlume::StaticDDSFlume(std::string portAddress, unsigned baudrate, bool safemode)
    : boostFlume(safemode, portAddress, baudrate)
    , SAFEMODE(safemode)
    , readComplete(true)
{
    boostFlume.setReadCallback(boost::bind(&StaticDDSFlume::readCallback, this, _1));
    boostFlume.setErrorCallback(boost::bind(&StaticDDSFlume::errorCallback, this, _1));
}
void StaticDDSFlume::write(std::string msg)
{
    readRegister.clear();
    errorMsg.clear();
    readComplete = false;

    // Ensure command ends with newline
    // if (!msg.empty() && msg.back() != '\n') {
    //     msg += '\n';
    // }
    std::vector<unsigned char> byteMsg(msg.begin(), msg.end());
    boostFlume.write(byteMsg);

    if (auto e = boostFlume.lastException()) {
        try {
            boost::rethrow_exception(e);
        } catch (boost::system::system_error& e) {
            throwNested("Error writing to serial port " + str(boostFlume.portID) + ": " + e.what());
        }
    }
}

std::string StaticDDSFlume::read()
{
    if (SAFEMODE) {
        return "static DDS is in safemode.";
    }

    std::string recv;
    for (auto idx : range(200)) {
        if (readComplete) {
            recv = std::string(readRegister.cbegin(), readRegister.cend());
            break;
        }
        Sleep(1);
    }

    if (recv.empty() || !readComplete) {
        thrower("Reading timed out or empty after 200ms from port " + str(boostFlume.portID));
    }

    if (!errorMsg.empty()) {
        thrower("Device returned error: " + errorMsg + "\nResponse: " + recv);
    }

    // Strip trailing newline
    recv.erase(std::remove(recv.begin(), recv.end(), '\n'), recv.end());
    return recv;
}

void StaticDDSFlume::resetConnection()
{
    boostFlume.disconnect();
    Sleep(10);
    boostFlume.reconnect();
}

void StaticDDSFlume::readCallback(int byte)
{
    if (byte < 0 || byte > 255) {
        thrower("Invalid byte read: must be in range 0–255.");
    }
    readRegister.push_back(static_cast<char>(byte));
    if (byte == '\n') {
        readComplete = true;
    }
}

void StaticDDSFlume::errorCallback(std::string error)
{
    errorMsg = error;
}

// --- Valon 5009B Specific Commands ---

void StaticDDSFlume::setFrequency(double frequencyMHz, int ch)
{
    // Command: Source ch; Frequency frequencyMHzM
    std::string cmd = "Source " + std::to_string(ch+1) + "; F " + std::to_string(frequencyMHz) + "\r";
    write(cmd);
}

void StaticDDSFlume::setOutputLevel(double plevel, int ch)
{
    // Command: Source <1|2>; ATT <0|31.5>
    std::string plevelStr;

   
    // Compute attenuation from 15 dBm
    double diff = 15.0 - plevel;

    // Round to nearest 0.25
    double att = std::round(diff / 0.25) * 0.25;

    plevelStr = std::to_string(att);


    std::string cmd = "Source " + std::to_string(ch + 1) + "; ATT " + plevelStr + "\r";
    write(cmd);
}

void StaticDDSFlume::setReferenceFrequency(double refMHz)
{
    std::string cmd = "REF " + std::to_string(refMHz);
    write(cmd);
}

std::string StaticDDSFlume::getSerialNumberOnly()
{
    for (int attempt = 0; attempt < 2; ++attempt) {
        write("ID?\r"); // sends ID?/r
        Sleep(5);
        read();
        std::string idn(readRegister.begin(), readRegister.end());
        // Split by commas
        size_t first = idn.find(',');
        if (first == std::string::npos) continue;
        size_t second = idn.find(',', first + 1);
        if (second == std::string::npos) continue;
        size_t third = idn.find(',', second + 1);
        if (third == std::string::npos) continue;

        // Serial number is between second and third comma
        std::string serial = idn.substr(second + 1, third - second - 1);

        // Trim whitespace
        serial.erase(0, serial.find_first_not_of(" \t\r\n"));
        serial.erase(serial.find_last_not_of(" \t\r\n") + 1);

        return serial;
    }
    return "Unknown";
}