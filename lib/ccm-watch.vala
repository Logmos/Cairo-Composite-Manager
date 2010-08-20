/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * maia-watch.vala
 * Copyright (C) Nicolas Bruguier 2010 <gandalfn@club-internet.fr>
 * 
 * libmaia is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * maiawm is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
public abstract class CCM.Watch : GLib.Object
{
    // Source event watcher
    private CCM.Source m_Source = null;
    // Poll FD connection watcher
    private GLib.PollFD? m_Fd = null;

    private bool
    on_source_prepare (out int inTimeout)
    {
        bool ret = false;

        inTimeout = -1;
        process_watch ();

        return ret;
    }

    private bool
    on_source_check ()
    {
        bool ret = false;

        if ((m_Fd.revents & IOCondition.NVAL) == IOCondition.NVAL)
        {
            m_Source = null;
        }
        else if ((m_Fd.revents & IOCondition.IN) == IOCondition.IN)
        {
            process_watch ();
            ret = true;
        }

        return ret;
    }

    private bool
    on_source_dispatch(SourceFunc inCallback)
    {
        bool ret = false;

        if ((m_Fd.revents & IOCondition.IN) == IOCondition.IN)
        {
            process_watch ();
            ret = true;
        }

        return ret;
    }

    private abstract void
    process_watch ();

    protected void
    watch (int inFd, GLib.MainContext? inContext = null)
    {
        SourceFuncs funcs = { on_source_prepare,
                              on_source_check,
                              on_source_dispatch,
                              null };

        m_Fd = PollFD();
        m_Fd.fd = inFd;
        m_Fd.events = IOCondition.IN;
        m_Source = new Source.from_pollfd (funcs, m_Fd, this); 
        m_Source.attach (inContext);
        m_Source.unref ();
    }
}
