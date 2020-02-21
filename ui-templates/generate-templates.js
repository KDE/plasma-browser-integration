/*
    Copyright (C) 2020 Kai Uwe Broulik <kde@broulik.de>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

const ejs = require("ejs"),
      fs = require("fs"),
      path = require("path");

function precompile(inFile, outFile) {
    const basename = path.basename(inFile, ".ejs");

    const inContents = fs.readFileSync(inFile, "utf8");

    const compiled = ejs.compile(inContents, {
        client: true, // make it standalone
        filename: inFile
    });

    // We could also make a new ejs.Template, compile(), and then get template.source
    // but it lacks some boilerplate around it (e.g. escapeFn and rethrow)
    // which we apparently can only get when dumping the entire function :(

    const lines = compiled.toString().split("\n");

    lines.splice(0, 1, lines[0].replace(/^function anonymous/, 'function ui_' + escape(basename)));

    const output = `/*
 * THIS FILE WAS AUTO-GENERATED, DO NOT EDIT!
 * See ui-templates/README.txt
 *
 * EJS Embedded JavaScript templates
 * Copyright 2112 Matthew Eernisse (mde@fleegix.org)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

` + lines.join("\n");

    fs.writeFileSync(outFile, output);
}

precompile("itinerary.ejs", "../extension/ui_itinerary.js");
