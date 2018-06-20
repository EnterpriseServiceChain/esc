#ifndef UNSETNODESTATUSHANDLER_H
#define UNSETNODESTATUSHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/unsetnodestatus.h"

class office;

class UnsetNodeStatusHandler : public CommandHandler {
  public:
    UnsetNodeStatusHandler(office& office, boost::asio::ip::tcp::socket& socket);

    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual ErrorCodes::Code onValidate() override;

  private:
    std::unique_ptr<UnsetNodeStatus>  m_command;
};

#endif // UNSETNODESTATUSHANDLER_H
