#include "broadcastmsghandler.h"
#include "command/broadcastmsg.h"
#include "../office.hpp"
#include "helper/hash.h"

BroadcastMsgHandler::BroadcastMsgHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void BroadcastMsgHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<BroadcastMsg>(dynamic_cast<BroadcastMsg*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("BroadcastMsg bad_cast caught: %s", bc.what());
        return;
    }

}

void BroadcastMsgHandler::onExecute() {
    assert(m_command);

    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    int64_t     fee{0};
    int64_t     deduct{0};

    deduct = m_command->getDeduct();
    fee = m_command->getFee();

    //commit changes
    m_usera.msid++;
    m_usera.time=m_command->getTime();
    m_usera.lpath=lpath;

    Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

    uint32_t msid;
    uint32_t mpos;

    if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
        DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        errorCode = ErrorCodes::Code::eMessageSubmitFail;
    } else {
        m_offi.set_user(m_command->getUserId(), m_usera, deduct+fee);

        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getAdditionalDataSize();
        tlog.user   = m_command->getUserId();
        tlog.umid   = m_command->getUserMessageId();
        tlog.nmid   = msid;
        tlog.mpos   = mpos;
        tlog.weight = -deduct;

        tInfo info;
        info.weight = m_usera.weight;
        info.deduct = m_command->getDeduct();
        info.fee = m_command->getFee();
        info.stat = m_usera.stat;
        memcpy(info.pkey, m_usera.pkey, sizeof(info.pkey));
        memcpy(tlog.info, &info, sizeof(tInfo));

        m_offi.put_ulog(m_command->getUserId(),  tlog);
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            commandresponse response{m_usera, msid, mpos};
            boost::asio::write(m_socket, boost::asio::buffer(&response, sizeof(response)));
        }
    } catch (std::exception&) {
        DLOG("ERROR responding to client %08X\n",m_usera.user);
    }
}

ErrorCodes::Code BroadcastMsgHandler::onValidate() {
    if (m_command->getAdditionalDataSize() > MAX_BROADCAST_LENGTH) {
        return ErrorCodes::Code::eBroadcastMaxLength;
    }
    return ErrorCodes::Code::eNone;
}

