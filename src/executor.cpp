#include <executor.hpp>

void Executor::run()
{
    try
    {
        if (m_ctx.stopped())
            m_ctx.restart();

        m_ctx.run();
    }
    catch (...)
    {
        m_ctx.stop();

        throw;
    }
}