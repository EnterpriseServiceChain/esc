#include "getloghandler.h"
#include "command/getlog.h"
#include "../office.hpp"
#include "helper/hash.h"
#include "helper/hlog.h"

GetLogHandler::GetLogHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetLogHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<GetLog>(dynamic_cast<GetLog*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("GetLog bad_cast caught: %s", bc.what());
        return;
    }
}

void GetLogHandler::onExecute() {
    assert(m_command);
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    std::string slog;
    if (!m_offi.get_log(m_command->getBankId(), m_command->getUserId(), m_command->getTime(), slog)) {
        errorCode = ErrorCodes::Code::eGetLogFailed;
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if (!errorCode) {
            boost::asio::write(m_socket, boost::asio::buffer(&m_usera, sizeof(user_t)));
            boost::asio::write(m_socket, boost::asio::buffer(slog.c_str(), slog.size()));
        }
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

bool GetLogHandler::onValidate() {
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    if(m_command->getBankId()!=m_offi.svid) {
        errorCode = ErrorCodes::Code::eBankNotFound;
    }

    if (errorCode) {
        try {
            boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        } catch (std::exception& e) {
            DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
        }
        return false;
    }

    return true;
}