/*
 * Copyright 2018 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Gabe Black
 */

#include "base/logging.hh"
#include "systemc/core/bindinfo.hh"
#include "systemc/core/module.hh"
#include "systemc/ext/core/sc_port.hh"

namespace sc_core
{

sc_port_base::sc_port_base(const char *name, int n, sc_port_policy p) :
    sc_object(name), _maxSize(n), _size(0), finalized(false)
{
    ::sc_gem5::Module *m = ::sc_gem5::currentModule();
    m->ports.push_back(this);
}

void
sc_port_base::warn_unimpl(const char *func) const
{
    warn("%s not implemented.\n", func);
}

int sc_port_base::maxSize() const { return _maxSize; }
int sc_port_base::size() const { return _size; }

void
sc_port_base::bind(sc_interface &i)
{
    _gem5BindInfo.push_back(new ::sc_gem5::BindInfo(&i));
}

void
sc_port_base::bind(sc_port_base &p)
{
    _gem5BindInfo.push_back(new ::sc_gem5::BindInfo(&p));
}

void
sc_port_base::_gem5Finalize()
{
    if (finalized)
        return;
    finalized = true;

    for (auto &bi: _gem5BindInfo) {
        if (bi->interface) {
            _size++;
            _gem5AddInterface(bi->interface);
        } else {
            sc_port_base *port = bi->port;
            port->_gem5Finalize();
            int size = port->size();
            for (int i = 0; i < size; i++) {
                _size++;
                _gem5AddInterface(port->_gem5Interface(i));
            }
        }
        delete bi;
    }

    _gem5BindInfo.clear();
}

} // namespace sc_core
