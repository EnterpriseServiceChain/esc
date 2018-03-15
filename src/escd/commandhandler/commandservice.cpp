#include "commandservice.h"
#include "command/getaccount.h"
#include "command/setaccountkey.h"
#include "command/sendone.h"
#include "command/sendmany.h"
#include "../office.hpp"

CommandService::CommandService(office& office, boost::asio::ip::tcp::socket& socket)
    : m_offi(office),
      m_getAccountHandler(office, socket),
      m_setAccountHandler(office, socket),
      m_createNodeHandler(office, socket),
      m_sendOneHandler(office, socket),
      m_sendManyHandler(office, socket),
      m_createAccountHandler(office, socket) {
}

void CommandService::onExecute(std::unique_ptr<IBlockCommand> command) {
    user_t usera;
    if(!m_offi.get_user(usera, command->getBankId(), command->getUserId())) {
        DLOG("ERROR: read user failed\n");
        return;
    };

    switch(command->getType()) {
    case TXSTYPE_INF:
        m_getAccountHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_KEY:
        m_setAccountHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_BNK:
        m_createNodeHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_PUT:
        m_sendOneHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_MPT:
        m_sendManyHandler.execute(std::move(command), std::move(usera));
    	break;
    case TXSTYPE_USR:
        m_createAccountHandler.execute(std::move(command), std::move(usera));
        break;
    default:
        DLOG("Command type: %d without handler\n", command->getType());
        break;
    }
}
