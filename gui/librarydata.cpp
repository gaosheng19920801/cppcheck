/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2015 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "librarydata.h"

#include <QDomDocument>
#include <QDomNode>
#include <QDomNodeList>

const unsigned int LibraryData::Function::Arg::ANY = ~0U;

LibraryData::LibraryData()
{
}


static LibraryData::Define loadDefine(const QDomElement &defineElement)
{
    LibraryData::Define define;
    define.name = defineElement.attribute("name");
    define.value = defineElement.attribute("value");
    return define;
}

static LibraryData::Function::Arg loadFunctionArg(const QDomElement &functionArgElement)
{
    LibraryData::Function::Arg arg;
    if (functionArgElement.attribute("nr") == "any")
        arg.nr = LibraryData::Function::Arg::ANY;
    else
        arg.nr = functionArgElement.attribute("nr").toUInt();
    for (QDomElement childElement = functionArgElement.firstChildElement(); !childElement.isNull(); childElement = childElement.nextSiblingElement()) {
        if (childElement.tagName() == "not-bool")
            arg.notbool = true;
        else if (childElement.tagName() == "not-null")
            arg.notnull = true;
        else if (childElement.tagName() == "not-uninit")
            arg.notuninit = true;
        else if (childElement.tagName() == "strz")
            arg.strz = true;
        else if (childElement.tagName() == "formatstr")
            arg.formatstr = true;
        else if (childElement.tagName() == "valid")
            arg.valid = childElement.text();
        else if (childElement.tagName() == "minsize") {
            arg.minsize.type = childElement.attribute("type");
            arg.minsize.arg  = childElement.attribute("arg");
            arg.minsize.arg2 = childElement.attribute("arg2");
        }
    }
    return arg;
}

static LibraryData::Function loadFunction(const QDomElement &functionElement)
{
    LibraryData::Function function;
    function.name = functionElement.attribute("name");
    for (QDomElement childElement = functionElement.firstChildElement(); !childElement.isNull(); childElement = childElement.nextSiblingElement()) {
        if (childElement.tagName() == "noreturn")
            function.noreturn = (childElement.text() == "true");
        else if (childElement.tagName() == "pure")
            function.gccPure = true;
        else if (childElement.tagName() == "const")
            function.gccConst = true;
        else if (childElement.tagName() == "leak-ignore")
            function.leakignore = true;
        else if (childElement.tagName() == "use-retval")
            function.useretval = true;
        else if (childElement.tagName() == "formatstr") {
            function.formatstr.scan   = childElement.attribute("scan");
            function.formatstr.secure = childElement.attribute("secure");
        } else if (childElement.tagName() == "arg") {
            const LibraryData::Function::Arg fa = loadFunctionArg(childElement);
            function.args.append(fa);
        } else {
            int x = 123;
            x++;

        }
    }
    return function;
}


static LibraryData::MemoryResource loadMemoryResource(const QDomElement &element)
{
    LibraryData::MemoryResource memoryresource;
    memoryresource.type = element.tagName();
    for (QDomElement childElement = element.firstChildElement(); !childElement.isNull(); childElement = childElement.nextSiblingElement()) {
        if (childElement.tagName() == "alloc") {
            LibraryData::MemoryResource::Alloc alloc;
            alloc.init = (childElement.attribute("init", "false") == "true");
            alloc.name = childElement.text();
            memoryresource.alloc.append(alloc);
        } else if (childElement.tagName() == "dealloc")
            memoryresource.dealloc.append(childElement.text());
        else if (childElement.tagName() == "use")
            memoryresource.use.append(childElement.text());
    }
    return memoryresource;
}

static LibraryData::PodType loadPodType(const QDomElement &element)
{
    LibraryData::PodType podtype;
    podtype.name = element.attribute("name");
    podtype.size = element.attribute("size");
    podtype.sign = element.attribute("sign");
    return podtype;
}


bool LibraryData::open(QIODevice &file)
{
    QDomDocument doc;
    if (!doc.setContent(&file))
        return false;

    QDomElement rootElement = doc.firstChildElement("def");
    for (QDomElement e = rootElement.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        if (e.tagName() == "define")
            defines.append(loadDefine(e));
        else if (e.tagName() == "function")
            functions.append(loadFunction(e));
        else if (e.tagName() == "memory" || e.tagName() == "resource")
            memoryresource.append(loadMemoryResource(e));
        else if (e.tagName() == "podtype")
            podtypes.append(loadPodType(e));
    }

    return true;
}



static QDomElement FunctionElement(QDomDocument &doc, const LibraryData::Function &function)
{
    QDomElement functionElement = doc.createElement("function");
    functionElement.setAttribute("name", function.name);
    if (!function.noreturn) {
        QDomElement e = doc.createElement("noreturn");
        e.appendChild(doc.createTextNode("false"));
        functionElement.appendChild(e);
    }
    if (function.useretval)
        functionElement.appendChild(doc.createElement("useretval"));
    if (function.leakignore)
        functionElement.appendChild(doc.createElement("leak-ignore"));

    // Argument info..
    foreach(const LibraryData::Function::Arg &arg, function.args) {
        QDomElement argElement = doc.createElement("arg");
        functionElement.appendChild(argElement);
        if (arg.nr == LibraryData::Function::Arg::ANY)
            argElement.setAttribute("nr", "any");
        else
            argElement.setAttribute("nr", arg.nr);
        if (arg.notbool)
            argElement.appendChild(doc.createElement("not-bool"));
        if (arg.notnull)
            argElement.appendChild(doc.createElement("not-null"));
        if (arg.notuninit)
            argElement.appendChild(doc.createElement("not-uninit"));
        if (arg.strz)
            argElement.appendChild(doc.createElement("strz"));
        if (arg.formatstr)
            argElement.appendChild(doc.createElement("formatstr"));

        if (!arg.valid.isEmpty()) {
            QDomElement e = doc.createElement("valid");
            e.appendChild(doc.createTextNode(arg.valid));
            argElement.appendChild(e);
        }

        if (!arg.minsize.type.isEmpty()) {
            QDomElement e = doc.createElement("minsize");
            e.setAttribute("type", arg.minsize.type);
            e.setAttribute("arg", arg.minsize.arg);
            if (!arg.minsize.arg2.isEmpty())
                e.setAttribute("arg2", arg.minsize.arg2);
            argElement.appendChild(e);
        }
    }

    return functionElement;
}

static QDomElement MemoryResourceElement(QDomDocument &doc, const LibraryData::MemoryResource &mr)
{
    QDomElement element = doc.createElement(mr.type);
    foreach(const LibraryData::MemoryResource::Alloc &alloc, mr.alloc) {
        QDomElement e = doc.createElement("alloc");
        if (alloc.init)
            e.setAttribute("init", "true");
        e.appendChild(doc.createTextNode(alloc.name));
        element.appendChild(e);
    }
    foreach(const QString &dealloc, mr.dealloc) {
        QDomElement e = doc.createElement("dealloc");
        e.appendChild(doc.createTextNode(dealloc));
        element.appendChild(e);
    }
    foreach(const QString &use, mr.use) {
        QDomElement e = doc.createElement("use");
        e.appendChild(doc.createTextNode(use));
        element.appendChild(e);
    }
    return element;
}


QString LibraryData::toString() const
{
    QDomDocument doc;
    QDomElement root = doc.createElement("def");
    doc.appendChild(root);
    root.setAttribute("format","2");

    foreach(const Define &define, defines) {
        QDomElement defineElement = doc.createElement("define");
        defineElement.setAttribute("name", define.name);
        defineElement.setAttribute("value", define.value);
        root.appendChild(defineElement);
    }

    foreach(const Function &function, functions) {
        root.appendChild(FunctionElement(doc, function));
    }

    foreach(const MemoryResource &mr, memoryresource) {
        root.appendChild(MemoryResourceElement(doc, mr));
    }

    foreach(const PodType &podtype, podtypes) {
        QDomElement podtypeElement = doc.createElement("podtype");
        podtypeElement.setAttribute("name", podtype.name);
        podtypeElement.setAttribute("size", podtype.size);
        podtypeElement.setAttribute("sign", podtype.sign);
        root.appendChild(podtypeElement);
    }

    return doc.toString();
}


