# File:      slp.py
# Abstract:  Python wrapper for OpenSLP
# Requires:  OpenSLP installation, Pyrex
#                                                   
# Author(s): Ganesan Rajagopal <rganesan@debian.org>
# 
# Copyright (C) 2003 The OpenSLP Project
#
# This program is released under same license as the OpenSLP project (BSD
# style without the advertising clause). Alternatively, you can use it under
# the Python License.

import _slp

SLP_FALSE = 0
SLP_TRUE = 1

class SLPError:
    """Exception raised when an SLP error is returned by the C API """
    SLP_OK = 0
    SLP_LAST_CALL = 1
    SLP_LANGUAGE_NOT_SUPPORTED = -1
    SLP_PARSE_ERROR = -2
    SLP_INVALID_REGISTRATION = -3
    SLP_SCOPE_NOT_SUPPORTED = -4
    SLP_AUTHENTICATION_ABSENT = -6
    SLP_AUTHENTICATION_FAILED = -7
    SLP_INVALID_UPDATE = -13
    SLP_REFRESH_REJECTED = -15
    SLP_NOT_IMPLEMENTED = -17
    SLP_BUFFER_OVERFLOW = -18
    SLP_NETWORK_TIMED_OUT = -19
    SLP_NETWORK_INIT_FAILED = -20
    SLP_MEMORY_ALLOC_FAILED = -21
    SLP_PARAMETER_BAD = -22
    SLP_NETWORK_ERROR = -23
    SLP_INTERNAL_SYSTEM_ERROR = -24
    SLP_HANDLE_IN_USE = -25
    SLP_TYPE_ERROR = -26
    
    def __init__(self, errcode):
        self.errcode = errcode

def slpstr(data):
    """slpstr(data) converts data into a string like the str() builtin
    function str() except that the conversion is in a format suitable for
    the SLP API. A list will be flattened by joining it with a comma. A
    hash will become a comma separated (key=val) pairs"""
    if type(data) == type(""):
        datastr = data
    elif type(data) == type([]):
        datastr = ",".join(data)
    elif type(data) == type({}):
        l = []
        for key, val in data.items():
            l.append("(%s=%s)" % (key, val))
        datastr = ",".join(l)
    else:
        datastr = str(data)
    return datastr

def open(lang = "", async = 0):
    return SLPHandle(lang, async)
    
class SLPHandle:
    """SLP Handle"""
    def __init__(self, lang = "", async = 0, cookie = None):
        self.slph = _slp.SLPConn(lang, async, cookie or self)
        self.errcode = SLPError.SLP_OK  # error code from callback
        self.srvtypes = []              # used by findsrvtypes
        self.services = []              # used by findsrvs
        self.attrs = {}                 # used by findattrs

    def __del__(self):
        self.slph.close()

    def close(self):
        self.slph.close()

    def __errcb(self, errcode, cookie):
        self.errcode = errcode

    def __srvtypecb(self, srvtypes, errcode, cookie):
        if errcode == SLPError.SLP_OK:
            self.srvtypes.extend(srvtypes.split(","))
        elif errcode == SLPError.SLP_LAST_CALL:
            pass
        else:
            self.errcode = errcode
            return SLP_FALSE
        return SLP_TRUE

    def __srvcb(self, srvurl, lifetime, errcode, cookie):
        if errcode == SLPError.SLP_OK:
            self.services.append((srvurl, lifetime))
        elif errcode == SLPError.SLP_LAST_CALL:
            pass
        else:
            self.errcode = errcode
            return SLP_FALSE
        return SLP_TRUE

    def __attrcb(self, attrs, errcode, cookie):
        if errcode == SLPError.SLP_OK:
            for attr in attrs.split(","):
                key, val = attr[1:-1].split("=")
                self.attrs[key] = val
        elif errcode == SLPError.SLP_LAST_CALL:
            pass
        else:
            self.errcode = errcode
            return SLP_FALSE
        return SLP_TRUE

    def register(self, srvurl, lifetime, attrs = ""):
        """register an SLP service"""
        attrstr = slpstr(attrs)
        err = self.slph.register(srvurl, lifetime, attrstr, self.__errcb)
        if err != SLPError.SLP_OK or self.errcode != SLPError.SLP_OK:
            raise SLPError(err or self.errcode)
                 
    def deregister(self, srvurl):
        """deregister an SLP service"""
        err = self.slph.deregister(srvurl, self.__errcb)
        if err != SLPError.SLP_OK or self.errcode != SLPError.SLP_OK:
            raise SLPError(err or self.errcode)
        
    def delattrs(self, srvurl, attrs = ""):
        """delete attributes from a SLP service URL (deprecated)"""
        attrstr = slpstr(attrs)
        err = self.slph.delattrs(srvurl, attrstr, self.__errcb)
        if err != SLPError.SLP_OK or self.errcode != SLPError.SLP_OK:
            raise SLPError(err or self.errcode)

    def findsrvtypes(self, na = "", scopelist = "default"):
        """find SLP service types"""
        err = self.slph.findsrvtypes(na, slpstr(scopelist), self.__srvtypecb)
        if err != SLPError.SLP_OK or self.errcode != SLPError.SLP_OK:
            raise SLPError(err or self.errcode)
        return self.srvtypes

    def findsrvs(self, srvtype, scopelist = "default", searchfilter = ""):
        """find SLP services matching the service type and searchfilter"""
        err = self.slph.findsrvs(srvtype, slpstr(scopelist), searchfilter,
                          self.__srvcb)
        if err != SLPError.SLP_OK or self.errcode != SLPError.SLP_OK:
            raise SLPError(err or self.errcode)
        return self.services

    def findattrs(self, srvurl, attrids = "", scopelist = "default"):
        """find attributes for the given SLP service URL or service type"""
        err = self.slph.findattrs(srvurl, slpstr(scopelist),
                                  slpstr(attrids), self.__attrcb) 
        if err != SLPError.SLP_OK or self.errcode != SLPError.SLP_OK:
            raise SLPError(err or self.errcode)
        return self.attrs


    def findscopes(self):
        """find supported SLP scopes"""
        try:
            scopestr = self.slph.findscopes()
            return scopestr.split(",")
        except EnvironmentError, err:
            raise SLPError(err.errno)

def getrefreshinterval():
    """get refresh interval"""
    return _slp.getrefreshinterval()

def parsesrvurl(srvurl):
    """Parse given service URL"""
    try:
        return _slp.parsesrvurl(srvurl)
    except EnvironmentError, err:
        raise SLPError(err.errno)

def escape(s, isTag):
    """Escape given string"""
    try:
        return _slp.escape(s)
    except EnvironmentError, err:
        raise SLPError(err.errno)

def unescape(s, isTag):
    """Unescape given string"""
    try:
        return _slp.unescape(s)
    except EnvironmentError, err:
        raise SLPError(err.errno)

def getprop(name):
    """Get a SLP property"""
    return _slp.getprop(name)

def setprop(name, value):
    """Set an SLP property"""
    _slp.setprop(name, value)

# Local Variables: 
# mode: python 
# End: 
