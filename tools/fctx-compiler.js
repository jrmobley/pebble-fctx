#!/usr/bin/env node
/*jshint
    latedef: nofunc,
    node: true
*/
/*global
    require: false,
    console: false,
    process: false
*/

var _ = require('underscore'),
    fs = require('fs'),
    xml2js = require('xml2js'),
    pathParser = require('svg-path-parser'),
    argv = require('minimist')(process.argv.slice(2));

if (argv._.length !== 1) {
    console.log('Usage: svg-compiler <input>');
    process.exit(1);
}
var filename = argv._[0],
    output = argv._[1];

require('colors');

console.log('\nSVG compiler for pebble-fctx'.bold.blue);
console.log('----------------------------\n'.bold.blue);

fs.readFile(filename, function (err, data) {
    'use strict';
    if (err) {
        console.log('failed to read %s because %s', filename, err);
        process.exit(1);
    }
    var parser = new xml2js.Parser({
        explicitArray: true,
        valueProcessors: [
            xml2js.processors.parseNumbers
        ]
    });
    parser.parseString(data, function (err, result) {
        var defs = result.svg.defs[0];

        _.each(defs.path, function (path) {
            console.log('path id %s', path.$.id);
            var packedPath = packPath(path),
                output = 'resources/' + path.$.id + '.fpath';
            fs.writeFile(output, packedPath, function (err) {
                if (err) throw err;
                console.log('Wrote %d bytes to %s', packedPath.length, output);
            });
        });

        _.each(defs.font, function (font) {
            console.log('font id %s', font.$.id);
            var packedFont = packFont(font),
                output = 'resources/' + font.$.id + '.ffont';
            fs.writeFile(output, packedFont, function (err) {
                if (err) throw err;
                console.log('Wrote %d bytes to %s', packedFont.length, output);
            });
        });

    });
});

function packPath(path) {
    /*jshint validthis: true */
    'use strict';
    var data = path.$.d || '',
        commands = pathParser(data),
        cursor = { emScale: 1, x: 0, y: 0, x0: 0, y0: 0 },
        packedCommands,
        packedPath;

    packedCommands = commands.map(packPathCommand, cursor);
    packedPath = Buffer.concat(packedCommands);

    return packedPath;
}

function packFont(font) {
    'use strict';
    var metadata = font['font-face'][0].$,
        glyphElements = font.glyph,
        unicodeRangePattern = /U\+([A-Fa-f0-9]+)-([A-Fa-f0-9]+)/,
        unicodeRange = unicodeRangePattern.exec(metadata['unicode-range']),
        unicodeRangeBegin = parseInt(unicodeRange[1], 16),
        unicodeRangeEnd = parseInt(unicodeRange[2], 16) + 1,
        packedFontHeader,
        glyphCount,
        glyphTable = [],
        glyphIndex = [],
        entryPointBegin = 0,
        entryPointEnd = 0,
        packedGlyphTable,
        packedGlyphIndex,
        pathDataSize,
        packedPathData,
        packedFont,
        errorCount = 0;

    if (metadata['units-per-em'] > 72) {
        metadata.emScale = 72 / metadata['units-per-em'];
    } else {
        metadata.emScale = 1;
    }

    /* Build the glyphTable.  This will be a *sparse* array of glyph objects,
       indexed by unicode entry points.  While we're at it, count the glyphs. */
    glyphCount = glyphElements.reduce(function (count, glyphElement) {
        var horizAdvX = Number.parseInt(glyphElement.$['horiz-adv-x'] || font.$['horiz-adv-x'], 10),
            entryPoint = entryPointForGlyph(glyphElement),
            paddedEntryPoint = entryPoint && padNumber(entryPoint, 16, 4, '0'),
            unicodeString = padString(glyphElement.$.unicode || '', 3, ' '),
            glyphName = glyphElement.$['glyph-name'],
            glyph = {};

        if (!entryPoint) {
            console.log('(%s) cannot determine entry point, discarded'.yellow, glyphName);
            return count;
        }

        if (entryPoint < unicodeRangeBegin || entryPoint >= unicodeRangeEnd) {
            console.log('U+%s %s (%s) out of range, discarded'.yellow,  paddedEntryPoint, unicodeString, glyphName);
            return count;
        }

        glyph.horizAdvX = Math.floor(horizAdvX * metadata.emScale * 16 + 0.5);
        try {
            glyph.pathData = packPathData.call(metadata, glyphElement);
        } catch (e) {
            console.error('U+%s %s (%s) error packing path data'.red, paddedEntryPoint, unicodeString, glyphName);
            glyph.pathData = new Buffer(0);
            ++errorCount;
        }
        glyphTable[entryPoint] = glyph;
        console.log('U+%s %s (%s)  %d bytes', paddedEntryPoint, unicodeString, glyphName, glyph.pathData.length);
        return count + 1;
    }, 0);

    /* Build the glyphIndex.  While we're at it, calculate the total size of the
       path data. */
    pathDataSize = glyphTable.reduce(function (offset, glyph, entryPoint) {

        /* If any entry points have been skipped.  Write the current range to
           the index and start a new range at this entry point. */
        if (entryPoint > entryPointEnd) {
            if (entryPointBegin < entryPointEnd) {
                glyphIndex.push({ begin: entryPointBegin, end: entryPointEnd });
            }
            entryPointBegin = entryPoint;
        }
        entryPointEnd = entryPoint + 1;

        /* Record the curent path data offset and increment to total. */
        glyph.pathDataOffset = offset;
        return offset + glyph.pathData.length;
    }, 0);
    glyphIndex.push({ begin: entryPointBegin, end: entryPointEnd });

    console.log('\nunicode range index:');
    glyphIndex.forEach(function (entry) {
        console.log('\tU+%s-%s', padNumber(entry.begin, 16, 4, '0'), padNumber(entry.end - 1, 16, 4, '0'));
    });

    /* At this point, we are done using the glyphTable in sparse format and it
       would really be easier to work with in condensed format. */
    glyphTable = glyphTable.filter(function (value) { return value !== undefined; });

    /* Pack all of the path data into a single buffer. */
    packedPathData = Buffer.concat(glyphTable.map(function (glyph) { return glyph.pathData; }), pathDataSize);

    /* Pack the glyph table. */
    packedGlyphTable = new Buffer(6 * glyphCount);
    glyphTable.reduce(function (offset, glyph, index) {
        packedGlyphTable.writeUIntLE(glyph.pathDataOffset,  offset + 0, 2);
        packedGlyphTable.writeUIntLE(glyph.pathData.length, offset + 2, 2);
        packedGlyphTable.writeUIntLE(glyph.horizAdvX,       offset + 4, 2);
        return offset + 6;
    }, 0);

    /* Pack the glyph index. */
    packedGlyphIndex = new Buffer(4 * glyphIndex.length);
    glyphIndex.forEach(function (entry, index) {
        packedGlyphIndex.writeUIntLE(entry.begin, 4 * index + 0, 2);
        packedGlyphIndex.writeUIntLE(entry.end,   4 * index + 2, 2);
    });

    /* Pack the font header. */
    metadata['glyph-index-length'] = glyphIndex.length;
    metadata['glyph-table-length'] = glyphTable.length;
    packedFontHeader = packObject.call(metadata, metadata, 'FFFUU', [ 'units-per-em', 'ascent', 'descent', 'glyph-index-length', 'glyph-table-length' ]);

    /* Pack up the entire font. */
    packedFont = Buffer.concat([packedFontHeader, packedGlyphIndex, packedGlyphTable, packedPathData]);

    console.log('font header : %d bytes', packedFontHeader.length);
    console.log('     index  : %d bytes', packedGlyphIndex.length);
    console.log('     table  : %d bytes', packedGlyphTable.length);
    console.log('     paths  : %d bytes', packedPathData.length);
    console.log('total size  : %d bytes', packedFont.length);
    if (errorCount > 0) {
        console.error('WARNING - %d errors encountered.  See above.'.red, errorCount);
    }
    return packedFont;
}

function padNumber(num, radix, width, padChar) {
    return padString(num.toString(radix).toUpperCase(), width, padChar);
}

function padString(str, width, padChar) {
    var pad = width - str.length + 1;
    return Array(+(pad > 0 && pad)).join(padChar) + str;
}

var multiCharUnicodeAttrs = {
    'fi': 0xFB01, 'fl': 0xFB02
};

/**
 * Determine the unicode entry point for glyph.  The 'unicode' string attribute
 * is checked against a lookup table of known ligature sequences and that entry
 * point is used if found.  Otherwise, if the 'unicode' attribute is a single
 * character, then that character code is used.  In all other cases (such as
 * a missing 'unicode' attribute or finding an unrecognized ligature string)
 * the return value is undefined (which is falsy).
 */
function entryPointForGlyph(glyph) {
    var unicode = glyph.$.unicode;
    if (unicode) {
        if (multiCharUnicodeAttrs.hasOwnProperty(unicode)) {
            return multiCharUnicodeAttrs[unicode];
        } else if (unicode.length === 1) {
            return unicode.charCodeAt(0);
        }
    }
}

/**
 * Encodes the path data found in the 'd' attribute of a <glyph> element.
 * @param glyph  A single parsed glyph tag from the SVG.
 * @return a Buffer containing the encoded data.  If the glyph does not have
 *           any path data, the Buffer will be empty.
 */
function packPathData(glyph) {
    /*jshint validthis: true */
    'use strict';
    var metadata = this,
        data = glyph.$.d || '',
        commands = pathParser(data),
        cursor = { emScale: metadata.emScale, x: 0, y: 0, x0: 0, y0: 0 },
        packedCommands,
        packedPath;

    //console.log('"%s"  d="%s"', glyph.$.unicode, data);
    packedCommands = commands.map(packPathCommand, cursor);
    packedPath = Buffer.concat(packedCommands);

    return packedPath;
}

function packPathCommand(cmd, index, commands) {
    /*jshint validthis: true */
    'use strict';
    var cursor = this,
        buffer = null;

    if (cmd.command === 'moveto') {
        buffer = packObject.call(cursor, cmd, 'CXY', ['code', 'x', 'y']);
        cursor.x = cursor.x0 = cmd.x;
        cursor.y = cursor.y0 = cmd.y;

    } else if (cmd.command === 'closepath') {
        buffer = packObject.call(cursor, cmd, 'C', ['code']);
        cursor.x = cursor.x0;
        cursor.y = cursor.y0;

    } else if (cmd.command === 'lineto') {
        buffer = packObject.call(cursor, cmd, 'CXY', ['code', 'x', 'y']);
        cursor.x = cmd.x;
        cursor.y = cmd.y;

    } else if (cmd.command === 'horizontal lineto') {
        buffer = packObject.call(cursor, cmd, 'CX', ['code', 'x']);
        cursor.x = cmd.x;

    } else if (cmd.command === 'vertical lineto') {
        buffer = packObject.call(cursor, cmd, 'CY', ['code', 'y']);
        cursor.y = cmd.y;

    } else if (cmd.command === 'curveto') {
        buffer = packObject.call(cursor, cmd, 'CXYXYXY', ['code', 'x1', 'y1', 'x2', 'y2', 'x', 'y']);
        cursor.x = cmd.x;
        cursor.y = cmd.y;
        cursor.cpx = cmd.x2;
        cursor.cpy = cmd.y2;

    } else if (cmd.command === 'smooth curveto') {
        buffer = packObject.call(cursor, cmd, 'CXYXY', ['code', 'x2', 'y2', 'x', 'y']);
        cursor.x = cmd.x;
        cursor.y = cmd.y;

    } else if (cmd.command === 'quadratic curveto') {
        buffer = packObject.call(cursor, cmd, 'CXYXY', ['code', 'x1', 'y1', 'x', 'y']);
        cursor.x = cmd.x;
        cursor.y = cmd.y;

    } else if (cmd.command === 'smooth quadratic curveto') {
        buffer = packObject.call(cursor, cmd, 'CXY', ['code', 'x', 'y']);
        cursor.x = cmd.x;
        cursor.y = cmd.y;

    } else if (cmd.command === 'elliptical arc') {
        buffer = packArcTo.call(cursor, cmd);
    }
    return buffer;
}

function packObject(obj, format, keys) {
    /*jshint validthis: true*/
    'use strict';
    var cursor = this,
        sizes = {C:2, X:2, Y:2, F:2, U:2},
        fixedPointScale = 16,
        fmtCodes = format.split(''),
        bufferSize = fmtCodes.reduce(function (size, fmt) {
            return size + sizes[fmt];
        }, 0),
        buffer = new Buffer(bufferSize),
        debug = '    ';

    keys.reduce(function (offset, key, index) {
        var fmt = format[index],
            val = obj[key],
            size = sizes[fmt];
        if (fmt === 'C') {
            debug += val.toUpperCase();
            val = val.toUpperCase().charCodeAt(0);
            buffer.writeIntLE(val, offset, size);
        } else if (fmt === 'U') {
            debug += ' ' + val;
            buffer.writeUIntLE(val, offset, size);
        } else {
            if (obj.relative) {
                if (fmt === 'X') {
                    val += cursor.x;
                    obj[key] = val;
                } else if (fmt === 'Y') {
                    val += cursor.y;
                    obj[key] = val;
                }
            }
            debug += ' ' + val;
            val = Math.floor(val * cursor.emScale * fixedPointScale + 0.5);
            buffer.writeIntLE(val, offset, size);
        }
        return offset + size;
    }, 0);

    //console.log(debug);
    return buffer;
}

function packArcTo(cmd) {
    var cursor = this,
        TAU = 2 * Math.PI;

    /* For reference, see the SVG Specification, Appendix F: Implementation Requirements
     * Section F.6 Elliptical arc implementation.
     * http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes
     */

    /*
     * F.6.2 - Out-of-range parameters.
     */

    /* If the endpoints (x1, y1) and (x2, y2) are identical, then this is
     * equivalent to omitting the elliptical arc segment entirely.
     */
    if (cursor.x === cmd.x && cursor.y === cmd.y) {
        return;
    }

    /* If rx = 0 or ry = 0 then this arc is treated as a straight line segment
     * (a "lineto") joining the endpoints.
     */
    if (cmd.rx === 0 || cmd.ry === 0) {
        cmd.code = 'L';
        return packObject.call(cursor, cmd, 'CXY', ['code', 'x', 'y']);
    }

    /* If rx or ry have negative signs, these are dropped;
     * the absolute value is used instead.
     */
    var rx = Math.abs(cmd.rx);
    var ry = Math.abs(cmd.ry);

    /* If rx, ry and φ are such that there is no solution (basically, the
     * ellipse is not big enough to reach from (x1, y1) to (x2, y2)) then the
     * ellipse is scaled up uniformly until there is exactly one solution
     * (until the ellipse is just big enough).
     * This requirement will be handled below, where we are doing the relevant math.
     */

    /* φ is taken mod 360 degrees. */
    var phi = (cmd.xAxisRotation % 360) * TAU / 360;
    var cosPhi = Math.cos(phi);
    var sinPhi = Math.sin(phi);

    /*
     * Section F.6.5 - Conversion from endpoint to center parameterization
     */

    /* Step 1 : Compute (x1', y1') - the transformed start point [F.6.5.1] */

    var dx2 = (cursor.x - cmd.x) / 2.0;
    var dy2 = (cursor.y - cmd.y) / 2.0;
    var x1 =  cosPhi * dx2 + sinPhi * dy2;
    var y1 = -sinPhi * dx2 + cosPhi * dy2;

    /* Step 2 : Compute (cx', cy') [F.6.5.2] */

    var rx_sq = rx * rx;
    var ry_sq = ry * ry;
    var x1_sq = x1 * x1;
    var y1_sq = y1 * y1;

    /* Here is where we handle out-of-range ellipse radii, as described above.  See F.6.6 */
    var radiiCheck = x1_sq / rx_sq + y1_sq / ry_sq;
    if (radiiCheck > 1) {
        rx = Math.sqrt(radiiCheck) * rx;
        ry = Math.sqrt(radiiCheck) * ry;
        rx_sq = rx * rx;
        ry_sq = ry * ry;
    }

    var sign = (cmd.largeArc === cmd.sweep) ? -1 : 1;
    var sq = ((rx_sq * ry_sq) - (rx_sq * y1_sq) - (ry_sq * x1_sq)) / ((rx_sq * y1_sq) + (ry_sq * x1_sq));
    sq = (sq < 0) ? 0 : sq;
    var coef = sign * Math.sqrt(sq);
    var cx1 = coef *  ((rx * y1) / ry);
    var cy1 = coef * -((ry * x1) / rx);

    /* Step 3 : Compute (cx, cy) from (cx', cy') [F.6.5.3] */

    var sx2 = (cursor.x + cmd.x) / 2.0;
    var sy2 = (cursor.y + cmd.y) / 2.0;
    var cx = sx2 + (cosPhi * cx1 - sinPhi * cy1);
    var cy = sy2 + (sinPhi * cx1 + cosPhi * cy1);

    /* Step 4 : Compute the angleStart and the angleExtent */

    /* F.6.5.4 */
    var ux = (x1 - cx1) / rx;
    var uy = (y1 - cy1) / ry;
    var vx = (-x1 - cx1) / rx;
    var vy = (-y1 - cy1) / ry;

    /* F.6.5.5 */
    var n = Math.sqrt((ux * ux) + (uy * uy));
    var p = ux; // (1 * ux) + (0 * uy)
    sign = (uy < 0) ? -1.0 : 1.0;
    var angleStart = sign * Math.acos(p / n);

    /* F.6.5.6 */
    n = Math.sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
    p = ux * vx + uy * vy;
    sign = (ux * vy - uy * vx < 0) ? -1.0 : 1.0;
    var angleExtent = sign * Math.acos(p / n);
    if (!cmd.sweep && angleExtent > 0) {
        angleExtent -= TAU;
    } else if (cmd.sweep && angleExtent < 0) {
        angleExtent += TAU;
    }
    angleExtent %= TAU;
    angleStart %= TAU;

    /* Now that we have re-parameterized the elliptical arc into a normalized
     * circular arc, we can calculate a poly-bezier that approximates the
     * normalized circular arc.
     */
    var beziers = arcToBeziers(angleStart, angleExtent);

    /* And finally, we transform the poly-bezier approximation of the normalized
     * circular arc such that it becomes an approximation of the original
     * elliptical arc.
     * Scale by the ellipse radii, rotate by the ellipse angle, translate by the ellipse center.
     */
    var transform = function (o, xp, yp) {
        var x = (o[xp] * rx * cosPhi - o[yp] * ry * sinPhi) + cx;
        var y = (o[xp] * rx * sinPhi + o[yp] * ry * cosPhi) + cy;
        o[xp] = x;
        o[yp] = y;
    };
    beziers.forEach(function (cmd) {
        transform(cmd, 'x', 'y');
        transform(cmd, 'x1', 'y1');
        transform(cmd, 'x2', 'y2');
    });

    /* Overwrite the final point of the poly-bezier to make sure it is exactly the point originally
     * specified by the 'arcTo' command.
     */
    beziers[beziers.length-1].x = cmd.x;
    beziers[beziers.length-1].y = cmd.y;

    /* Pack the 'curveTo' commands. */
    var buffers = [];
    beziers.forEach(function (cmd) {
        buffers.push(packObject.call(cursor, cmd, 'CXYXYXY', ['code', 'x1', 'y1', 'x2', 'y2', 'x', 'y']));
        cursor.x = cmd.x;
        cursor.y = cmd.y;
        cursor.cpx = cmd.x2;
        cursor.cpy = cmd.y2;
    });

    var buffer = Buffer.concat(buffers);
    return buffer;
}


/*
* Generate the control points and endpoints for a set of bezier curves that match
* a circular arc starting from angle 'angleStart' and sweep the angle 'angleExtent'.
* The circle the arc follows will be centred on (0,0) and have a radius of 1.0.
*
* Each bezier can cover no more than 90 degrees, so the arc will be divided evenly
* into a maximum of four curves.
*
* The resulting control points will later be scaled and rotated to match the final
* arc required.
*
* The returned array has the format [x0,y0, x1,y1,...] and excludes the start point
* of the arc.
*/
function arcToBeziers(angleStart, angleExtent) {
   var numSegments = Math.ceil(Math.abs(angleExtent) / 90.0);

   var angleIncrement = (angleExtent / numSegments);

  // The length of each control point vector is given by the following formula.
  var controlLength = 4.0 / 3.0 * Math.sin(angleIncrement / 2.0) / (1.0 + Math.cos(angleIncrement / 2.0));

  var commands = [];

  for (var i = 0; i < numSegments; i++) {
      var cmd = { command: 'curveTo', code: 'C' };
      var angle = angleStart + i * angleIncrement;
      // Calculate the control vector at this angle
      var dx = Math.cos(angle);
      var dy = Math.sin(angle);
      // First control point
      cmd.x1 = (dx - controlLength * dy);
      cmd.y1 = (dy + controlLength * dx);
      // Second control point
      angle += angleIncrement;
      dx = Math.cos(angle);
      dy = Math.sin(angle);
      cmd.x2 = (dx + controlLength * dy);
      cmd.y2 = (dy - controlLength * dx);
      // Endpoint of bezier
      cmd.x = dx;
      cmd.y = dy;
      commands.push(cmd);
  }
  return commands;
}
