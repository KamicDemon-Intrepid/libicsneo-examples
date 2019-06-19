#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

#include "icsneo/icsneocpp.h"

int main() {
	// Print version
	std::cout << "Running libicsneo " << icsneo::GetVersion() << std::endl;
	
	std::cout<< "Supported devices:" << std::endl;
	for(auto& dev : icsneo::GetSupportedDevices())
		std::cout << '\t' << dev << std::endl;

	std::cout << "\nFinding devices... " << std::flush;
	auto devices = icsneo::FindAllDevices(); // This is type std::vector<std::shared_ptr<icsneo::Device>>
	// You now hold the shared_ptrs for these devices, you are considered to "own" these devices from a memory perspective
	std::cout << "OK, " << devices.size() << " device" << (devices.size() == 1 ? "" : "s") << " found" << std::endl;

	// List off the devices
	for(auto& device : devices)
		std::cout << '\t' << device->getType() << " - " << device->getSerial() << " @ Handle " << device->getNeoDevice().handle << std::endl;
	std::cout << std::endl;

	for(auto& device : devices) {
		std::cout << "Connecting to " << device->getType() << ' ' << device->getSerial() << "... ";
		bool ret = device->open();
		if(!ret) { // Failed to open
			std::cout << "FAIL" << std::endl;
			for(auto& err : icsneo::GetErrors())
				std::cout << "\t" << err << "\n";
			std::cout << std::endl;
			continue;
		}
		std::cout << "OK" << std::endl;

		std::cout << "\tGetting HSCAN Baudrate... ";
		int64_t baud = device->settings->getBaudrateFor(icsneo::Network::NetID::HSCAN);
		if(baud < 0)
			std::cout << "FAIL" << std::endl;
		else
			std::cout << "OK, " << (baud/1000) << "kbit/s" << std::endl;

		std::cout << "\tSetting HSCAN to operate at 125kbit/s... ";
		ret = device->settings->setBaudrateFor(icsneo::Network::NetID::HSCAN, 125000);
		std::cout << (ret ? "OK" : "FAIL") << std::endl;

		// Changes to the settings do not take affect until you call settings->apply()!
		// When you get the baudrate here, you're reading what the device is currently operating on
		std::cout << "\tGetting HSCAN Baudrate... (expected to be unchanged) ";
		baud = device->settings->getBaudrateFor(icsneo::Network::NetID::HSCAN);
		if(baud < 0)
			std::cout << "FAIL" << std::endl;
		else
			std::cout << "OK, " << (baud/1000) << "kbit/s" << std::endl;

		std::cout << "\tGetting HSCANFD Baudrate... ";
		baud = device->settings->getFDBaudrateFor(icsneo::Network::NetID::HSCAN);
		if(baud < 0)
			std::cout << "FAIL" << std::endl;
		else
			std::cout << "OK, " << (baud/1000) << "kbit/s" << std::endl;

		std::cout << "\tSetting HSCANFD to operate at 8Mbit/s... ";
		ret = device->settings->setFDBaudrateFor(icsneo::Network::NetID::HSCAN, 8000000);
		std::cout << (ret ? "OK" : "FAIL") << std::endl;

		std::cout << "\tGetting HSCANFD Baudrate... (expected to be unchanged) ";
		baud = device->settings->getFDBaudrateFor(icsneo::Network::NetID::HSCAN);
		if(baud < 0)
			std::cout << "FAIL" << std::endl;
		else
			std::cout << "OK, " << (baud/1000) << "kbit/s" << std::endl;

		// Setting settings temporarily does not need to be done before committing to device EEPROM
		// It's done here to test both functionalities
		// Setting temporarily will keep these settings until another send/commit is called or a power cycle occurs
		std::cout << "\tSetting settings temporarily... ";
		ret = device->settings->apply(true);
		std::cout << (ret ? "OK" : "FAIL") << std::endl;

		// Now that we have applied, we expect that our operating baudrates have changed
		std::cout << "\tGetting HSCAN Baudrate... ";
		baud = device->settings->getBaudrateFor(icsneo::Network::NetID::HSCAN);
		if(baud < 0)
			std::cout << "FAIL" << std::endl;
		else
			std::cout << "OK, " << (baud/1000) << "kbit/s" << std::endl;
		
		std::cout << "\tGetting HSCANFD Baudrate... ";
		baud = device->settings->getFDBaudrateFor(icsneo::Network::NetID::HSCAN);
		if(baud < 0)
			std::cout << "FAIL" << std::endl;
		else
			std::cout << "OK, " << (baud/1000) << "kbit/s" << std::endl;

		std::cout << "\tSetting settings permanently... ";
		ret = device->settings->apply();
		std::cout << (ret ? "OK\n\n" : "FAIL\n\n");

		// The concept of going "online" tells the connected device to start listening, i.e. ACKing traffic and giving it to us
		std::cout << "\tGoing online... ";
		ret = device->goOnline();
		if(!ret) {
			std::cout << "FAIL" << std::endl;
			device->close();
			continue;
		}
		std::cout << "OK" << std::endl;

		// A real application would just check the result of icsneo_goOnline() rather than calling this
		// This function is intended to be called later on if needed
		std::cout << "\tChecking online status... ";
		ret = device->isOnline();
		if(!ret) {
			std::cout << "FAIL\n" << std::endl;
			device->close();
			continue;
		}
		std::cout << "OK" << std::endl;

		// Now we can either register a handler (or multiple) for messages coming in
		// or we can enable message polling, and then call device->getMessages periodically

		// We're actually going to do both here, so first enable message polling
		device->enableMessagePolling();
		device->setPollingMessageLimit(100000); // Feel free to set a limit if you like, the default is a conservative 20k
		// Keep in mind that 20k messages comes quickly at high bus loads!

		// We can also register a handler
		std::cout << "\tStreaming messages in for 3 seconds... " << std::endl;
		// MessageCallbacks are powerful, and can filter on things like ArbID for you. See the documentation
		auto handler = device->addMessageCallback(icsneo::MessageCallback([](std::shared_ptr<icsneo::Message> message) {
			switch(message->network.getType()) {
				case icsneo::Network::Type::CAN: {
					// A message of type CAN is guaranteed to be a CANMessage, so we can static cast safely
					auto canMessage = std::static_pointer_cast<icsneo::CANMessage>(message);
					
					std::cout << "\t\tCAN ";
					if(canMessage->isCANFD) {
						std::cout << "FD ";
						if(!canMessage->baudrateSwitch)
							std::cout << "(No BRS) ";
					}
					
					// Print the Arbitration ID
					std::cout << "0x" << std::hex << std::setw(canMessage->isExtended ? 8 : 3) << std::setfill('0') << canMessage->arbid;
					
					// Print the DLC
					std::cout << std::dec << " [" << canMessage->data.size() << "] ";
					
					// Print the data
					for(auto& databyte : canMessage->data)
						std::cout << std::hex << std::setw(2) << (uint32_t)databyte << ' ';
					
					// Print the timestamp
					std::cout << std::dec << '(' << canMessage->timestamp << " ns since 1/1/2007)\n";
					break;
				}
				case icsneo::Network::Type::Ethernet: {
					auto ethMessage = std::static_pointer_cast<icsneo::EthernetMessage>(message);
					
					std::cout << "\t\t" << ethMessage->network << " Frame - " << std::dec << ethMessage->data.size() << " bytes on wire\n";
					std::cout << "\t\t  Timestamped:\t"<< ethMessage->timestamp << " ns since 1/1/2007\n";
					
					// The MACAddress may be printed directly or accessed with the `data` member
					std::cout << "\t\t  Source:\t" << ethMessage->getSourceMAC() << "\n";
					std::cout << "\t\t  Destination:\t" << ethMessage->getDestinationMAC();
					
					// Print the data
					for(size_t i = 0; i < ethMessage->data.size(); i++) {
						if(i % 8 == 0)
							std::cout << "\n\t\t  " << std::hex << std::setw(4) << std::setfill('0') << i << '\t';
						std::cout << std::hex << std::setw(2) << (uint32_t)ethMessage->data[i] << ' ';
					}

					std::cout << std::dec << std::endl;
					break;
				}
				default:
					// Ignoring non-network messages
					break;
			}
		}));
		std::this_thread::sleep_for(std::chrono::seconds(3));
		device->removeMessageCallback(handler); // Removing the callback means it will not be called anymore

		// Since we're using message polling, we can also get the messages which have come in for the past 3 seconds that way
		// We could simply call getMessages and it would return a vector of message pointers to us
		//auto messages = device->getMessages();

		// For speed when calling repeatedly, we can also preallocate and continually reuse a vector
		std::vector<std::shared_ptr<icsneo::Message>> messages;
		messages.reserve(100000);
		device->getMessages(messages);
		std::cout << "\t\tGot " << messages.size() << " messages while polling" << std::endl;

		// If we wanted to make sure it didn't grow and reallocate, we could also pass in a limit
		// If there are more messages than the limit, we can call getMessages repeatedly
		//device->getMessages(messages, 100);

		// You are now the owner (or one of the owners, if multiple handlers are registered) of the shared_ptrs to the messages
		// This means that when you let them go out of scope or reuse the vector, the messages will be freed automatically

		// We can transmit messages
		std::cout << "\tTransmitting an extended CAN FD frame... ";
		auto txMessage = std::make_shared<icsneo::CANMessage>();
		txMessage->network = icsneo::Network::NetID::HSCAN;
		txMessage->arbid = 0x1C5001C5;
		txMessage->data.insert(txMessage->data.end(), {0xaa, 0xbb, 0xcc});
		// The DLC will come from the length of the data vector
		txMessage->isExtended = true;
		txMessage->isCANFD = true;
		ret = device->transmit(txMessage); // This will return false if the device does not support CAN FD, or does not have HSCAN
		std::cout << (ret ? "OK" : "FAIL") << std::endl;

		std::cout << "\tTransmitting an ethernet frame on OP (BR) Ethernet 2... ";
		auto ethTxMessage = std::make_shared<icsneo::EthernetMessage>();
		ethTxMessage->network = icsneo::Network::NetID::OP_Ethernet2;
		ethTxMessage->data.insert(ethTxMessage->data.end(), {
			0x00, 0xFC, 0x70, 0x00, 0x01, 0x02, /* Destination MAC */
			0x00, 0xFC, 0x70, 0x00, 0x01, 0x01, /* Source MAC */
			0x00, 0x00, /* Ether Type */
			0x01, 0xC5, 0x01, 0xC5 /* Payload (will automatically be padded on transmit unless you set `ethTxMessage->noPadding`) */
		});
		ret = device->transmit(ethTxMessage); // This will return false if the device does not support OP (BR) Ethernet 2
		std::cout << (ret ? "OK" : "FAIL") << std::endl;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// Go offline, stop sending and receiving traffic
		std::cout << "\tGoing offline... ";
		ret = device->goOffline();
		std::cout << (ret ? "OK" : "FAIL") << std::endl;

		// Apply default settings
		std::cout << "\tSetting default settings... ";
		ret = device->settings->applyDefaults(); // This will also write to the device
		std::cout << (ret ? "OK" : "FAIL") << std::endl;

		std::cout << "\tDisconnecting... ";
		ret = device->close();
		std::cout << (ret ? "OK\n" : "FAIL\n") << std::endl;
	}
	
	std::cout << "Press any key to continue..." << std::endl;
	std::cin.get();
	return 0;
}