/*
 * Copyright (C) 2020-2021, Jan Grulich
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3.0 of the
 * License.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "portal.h"
#include "parent.h"
#include "parent_p.h"
#include "portal_p.h"

#include <QPrinter>
#include <QPageSize>
#include <QUrl>

namespace Xdp
{

// INFO: code below is copied from Qt as there is no public API for converting key to PageSizeId
// Standard sizes data
struct StandardPageSize {
    QPageSize::PageSizeId id;
    const char *mediaOption; // PPD standard mediaOption ID
};

// Standard page sizes taken from the Postscript PPD Standard v4.3
// See https://www-cdf.fnal.gov/offline/PostScript/5003.PPD_Spec_v4.3.pdf
// Excludes all Transverse and Rotated sizes
// NB!This table needs to be in sync with QPageSize::PageSizeId
const static StandardPageSize qt_pageSizes[] = {

    // Existing Qt sizes including ISO, US, ANSI and other standards
    {QPageSize::A4, "A4"},
    {QPageSize::B5, "ISOB5"},
    {QPageSize::Letter, "Letter"},
    {QPageSize::Legal, "Legal"},
    {QPageSize::Executive, "Executive.7.5x10in"}, // Qt size differs from Postscript / Windows
    {QPageSize::A0, "A0"},
    {QPageSize::A1, "A1"},
    {QPageSize::A2, "A2"},
    {QPageSize::A3, "A3"},
    {QPageSize::A5, "A5"},
    {QPageSize::A6, "A6"},
    {QPageSize::A7, "A7"},
    {QPageSize::A8, "A8"},
    {QPageSize::A9, "A9"},
    {QPageSize::B0, "ISOB0"},
    {QPageSize::B1, "ISOB1"},
    {QPageSize::B10, "ISOB10"},
    {QPageSize::B2, "ISOB2"},
    {QPageSize::B3, "ISOB3"},
    {QPageSize::B4, "ISOB4"},
    {QPageSize::B6, "ISOB6"},
    {QPageSize::B7, "ISOB7"},
    {QPageSize::B8, "ISOB8"},
    {QPageSize::B9, "ISOB9"},
    {QPageSize::C5E, "EnvC5"},
    {QPageSize::Comm10E, "Env10"},
    {QPageSize::DLE, "EnvDL"},
    {QPageSize::Folio, "Folio"},
    {QPageSize::Ledger, "Ledger"},
    {QPageSize::Tabloid, "Tabloid"},
    {QPageSize::Custom, "Custom"}, // Special case to keep in sync with QPageSize::PageSizeId

    // ISO Standard Sizes
    {QPageSize::A10, "A10"},
    {QPageSize::A3Extra, "A3Extra"},
    {QPageSize::A4Extra, "A4Extra"},
    {QPageSize::A4Plus, "A4Plus"},
    {QPageSize::A4Small, "A4Small"},
    {QPageSize::A5Extra, "A5Extra"},
    {QPageSize::B5Extra, "ISOB5Extra"},

    // JIS Standard Sizes
    {QPageSize::JisB0, "B0"},
    {QPageSize::JisB1, "B1"},
    {QPageSize::JisB2, "B2"},
    {QPageSize::JisB3, "B3"},
    {QPageSize::JisB4, "B4"},
    {QPageSize::JisB5, "B5"},
    {QPageSize::JisB6, "B6"},
    {QPageSize::JisB7, "B7"},
    {QPageSize::JisB8, "B8"},
    {QPageSize::JisB9, "B9"},
    {QPageSize::JisB10, "B10"},

    // ANSI / US Standard sizes
    {QPageSize::AnsiC, "AnsiC"},
    {QPageSize::AnsiD, "AnsiD"},
    {QPageSize::AnsiE, "AnsiE"},
    {QPageSize::LegalExtra, "LegalExtra"},
    {QPageSize::LetterExtra, "LetterExtra"},
    {QPageSize::LetterPlus, "LetterPlus"},
    {QPageSize::LetterSmall, "LetterSmall"},
    {QPageSize::TabloidExtra, "TabloidExtra"},

    // Architectural sizes
    {QPageSize::ArchA, "ARCHA"},
    {QPageSize::ArchB, "ARCHB"},
    {QPageSize::ArchC, "ARCHC"},
    {QPageSize::ArchD, "ARCHD"},
    {QPageSize::ArchE, "ARCHE"},

    // Inch-based Sizes
    {QPageSize::Imperial7x9, "7x9"},
    {QPageSize::Imperial8x10, "8x10"},
    {QPageSize::Imperial9x11, "9x11"},
    {QPageSize::Imperial9x12, "9x12"},
    {QPageSize::Imperial10x11, "10x11"},
    {QPageSize::Imperial10x13, "10x13"},
    {QPageSize::Imperial10x14, "10x14"},
    {QPageSize::Imperial12x11, "12x11"},
    {QPageSize::Imperial15x11, "15x11"},

    // Other Page Sizes
    {QPageSize::ExecutiveStandard, "Executive"}, // Qt size differs from Postscript / Windows
    {QPageSize::Note, "Note"},
    {QPageSize::Quarto, "Quarto"},
    {QPageSize::Statement, "Statement"},
    {QPageSize::SuperA, "SuperA"},
    {QPageSize::SuperB, "SuperB"},
    {QPageSize::Postcard, "Postcard"},
    {QPageSize::DoublePostcard, "DoublePostcard"},
    {QPageSize::Prc16K, "PRC16K"},
    {QPageSize::Prc32K, "PRC32K"},
    {QPageSize::Prc32KBig, "PRC32KBig"},

    // Fan Fold Sizes
    {QPageSize::FanFoldUS, "FanFoldUS"},
    {QPageSize::FanFoldGerman, "FanFoldGerman"},
    {QPageSize::FanFoldGermanLegal, "FanFoldGermanLegal"},

    // ISO Envelopes
    {QPageSize::EnvelopeB4, "EnvISOB4"},
    {QPageSize::EnvelopeB5, "EnvISOB5"},
    {QPageSize::EnvelopeB6, "EnvISOB6"},
    {QPageSize::EnvelopeC0, "EnvC0"},
    {QPageSize::EnvelopeC1, "EnvC1"},
    {QPageSize::EnvelopeC2, "EnvC2"},
    {QPageSize::EnvelopeC3, "EnvC3"},
    {QPageSize::EnvelopeC4, "EnvC4"},
    {QPageSize::EnvelopeC6, "EnvC6"},
    {QPageSize::EnvelopeC65, "EnvC65"},
    {QPageSize::EnvelopeC7, "EnvC7"},

    // US Envelopes
    {QPageSize::Envelope9, "Env9"},
    {QPageSize::Envelope11, "Env11"},
    {QPageSize::Envelope12, "Env12"},
    {QPageSize::Envelope14, "Env14"},
    {QPageSize::EnvelopeMonarch, "EnvMonarch"},
    {QPageSize::EnvelopePersonal, "EnvPersonal"},

    // Other Envelopes
    {QPageSize::EnvelopeChou3, "EnvChou3"},
    {QPageSize::EnvelopeChou4, "EnvChou4"},
    {QPageSize::EnvelopeInvite, "EnvInvite"},
    {QPageSize::EnvelopeItalian, "EnvItalian"},
    {QPageSize::EnvelopeKaku2, "EnvKaku2"},
    {QPageSize::EnvelopeKaku3, "EnvKaku3"},
    {QPageSize::EnvelopePrc1, "EnvPRC1"},
    {QPageSize::EnvelopePrc2, "EnvPRC2"},
    {QPageSize::EnvelopePrc3, "EnvPRC3"},
    {QPageSize::EnvelopePrc4, "EnvPRC4"},
    {QPageSize::EnvelopePrc5, "EnvPRC5"},
    {QPageSize::EnvelopePrc6, "EnvPRC6"},
    {QPageSize::EnvelopePrc7, "EnvPRC7"},
    {QPageSize::EnvelopePrc8, "EnvPRC8"},
    {QPageSize::EnvelopePrc9, "EnvPRC9"},
    {QPageSize::EnvelopePrc10, "EnvPRC10"},
    {QPageSize::EnvelopeYou4, "EnvYou4"}};

// Return key name for PageSize
static QString qt_keyForPageSizeId(QPageSize::PageSizeId id)
{
    return QString::fromLatin1(qt_pageSizes[id].mediaOption);
}

static GVariant *settingsFromPrinter(const QPrinter &printer)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

    g_variant_builder_add(&builder, "{sv}", "n-copies", g_variant_new_int32(printer.copyCount()));

    QString orientation = QStringLiteral("landscape");
    if (printer.pageLayout().orientation() == QPageLayout::Portrait) {
        orientation = QStringLiteral("portrait");
    }
    g_variant_builder_add(&builder, "{sv}", "orientation", g_variant_new_string(orientation.toUtf8().constData()));



    if (printer.resolution()) {
        g_variant_builder_add(&builder, "{sv}", "resolution", g_variant_new_int32(printer.resolution()));
    }

    g_variant_builder_add(&builder, "{sv}", "use-color", g_variant_new_string(printer.colorMode() == QPrinter::Color ? "yes" : "no"));

    QString duplex = QStringLiteral("simplex");
    if (printer.duplex() == QPrinter::DuplexShortSide) {
        duplex = QStringLiteral("horizontal");
    } else if (printer.duplex() == QPrinter::DuplexLongSide) {
        duplex = QStringLiteral("vertical");
    }
    g_variant_builder_add(&builder, "{sv}", "duplex", g_variant_new_string(duplex.toUtf8().constData()));

    g_variant_builder_add(&builder, "{sv}", "collate", g_variant_new_string(printer.collateCopies() ? "yes" : "no"));
    g_variant_builder_add(&builder, "{sv}", "reverse", g_variant_new_string(printer.pageOrder() == QPrinter::LastPageFirst ? "yes" : "no"));

    QString range = QStringLiteral("all");
    if (printer.printRange() == QPrinter::Selection) {
        range = QStringLiteral("selection");
    } else if (printer.printRange() == QPrinter::CurrentPage) {
        range = QStringLiteral("current");
    } else if (printer.printRange() == QPrinter::PageRange) {
        range = QStringLiteral("ranges");
    }
    g_variant_builder_add(&builder, "{sv}", "print-pages", g_variant_new_string(range.toUtf8().constData()));

    if (printer.fromPage() || printer.toPage()) {
        g_variant_builder_add(&builder, "{sv}", "page-ranges", g_variant_new_string(QStringLiteral("%1-%2").arg(printer.fromPage()).arg(printer.toPage()).toUtf8().constData()));
    }

    g_variant_builder_add(&builder, "{sv}", "output-file-format", g_variant_new_string("pdf"));
    g_variant_builder_add(&builder, "{sv}", "output-uri", g_variant_new_string(printer.outputFileName().toUtf8().constData()));

    return g_variant_builder_end(&builder);
}


static GVariant *pageSetupFromPrinter(const QPrinter &printer)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

    const QPageLayout layout = printer.pageLayout();
    QString orientation = QStringLiteral("landscape");
    if (printer.pageLayout().orientation() == QPageLayout::Portrait) {
        orientation = QStringLiteral("portrait");
    }
    g_variant_builder_add(&builder, "{sv}", "Orientation", g_variant_new_string(orientation.toUtf8().constData()));
    g_variant_builder_add(&builder, "{sv}", "PPDName", g_variant_new_string(qt_keyForPageSizeId(layout.pageSize().id()).toUtf8().constData()));
    g_variant_builder_add(&builder, "{sv}", "DisplayName", g_variant_new_string(layout.pageSize().name().toUtf8().constData()));
    g_variant_builder_add(&builder, "{sv}", "Width", g_variant_new_double(layout.pageSize().size(QPageSize::Millimeter).width()));
    g_variant_builder_add(&builder, "{sv}", "Height", g_variant_new_double(layout.pageSize().size(QPageSize::Millimeter).height()));

    const QMarginsF pageMargins = layout.margins(QPageLayout::Millimeter);
    g_variant_builder_add(&builder, "{sv}", "MarginTop", g_variant_new_double(pageMargins.top()));
    g_variant_builder_add(&builder, "{sv}", "MarginBottom", g_variant_new_double(pageMargins.bottom()));
    g_variant_builder_add(&builder, "{sv}", "MarginLeft", g_variant_new_double(pageMargins.left()));
    g_variant_builder_add(&builder, "{sv}", "MarginRight", g_variant_new_double(pageMargins.right()));

    return g_variant_builder_end(&builder);
}

void PortalPrivate::preparePrint(const Parent &parent, const QString &title, const QPrinter &printer, PrintFlags flags)
{
    // FIXME: not processed for missing API or private API
    // Settings: paper-format, paper-width, paper-height, default-source, quality, media-type, dither, scale,
    //           page-set, finishings, number-up, number-up-layout, output-bin, resolution-x, resolution-y, printer-lpi
    //           output-file-format
    xdp_portal_prepare_print(m_xdpPortal, parent.d_ptr->m_xdpParent, title.toUtf8().constData(), settingsFromPrinter(printer),
                             pageSetupFromPrinter(printer), static_cast<XdpPrintFlags>((int)flags), nullptr, preparedPrint, this);
}

void PortalPrivate::preparedPrint(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;
    g_autoptr(GVariant) ret = xdp_portal_prepare_print_finish(xdpPortal, result, &error);

    if (ret) {
        QVariantMap responseData;
        guint token = 0;
        g_autoptr(GVariant) settings;
        g_autoptr(GVariant) pageSetup;

        QVariantMap settingsMap;
        QVariantMap pageSetupMap;

        settings = g_variant_lookup_value(ret, "settings", G_VARIANT_TYPE_DICTIONARY);

        if (settings) {
            gint nCopies = 0;
            if (g_variant_lookup(settings, "n-copies", "i", &nCopies)) {
                settingsMap.insert(QStringLiteral("n-copies"), nCopies);
            }

            g_autofree gchar *orientation = nullptr;
            if (g_variant_lookup(settings, "orientation", "s", &orientation)) {
                settingsMap.insert(QStringLiteral("orientation"), orientation);
            }

            gint resolution = 0;
            if (g_variant_lookup(settings, "resolution", "i", &resolution)) {
                settingsMap.insert(QStringLiteral("resolution"), resolution);
            }

            g_autofree gchar *useColor = nullptr;
            if (g_variant_lookup(settings, "use-color", "s", &useColor)) {
                settingsMap.insert(QStringLiteral("use-color"), useColor);
            }

            g_autofree gchar *duplex = nullptr;
            if (g_variant_lookup(settings, "duplex", "s", &duplex)) {
                settingsMap.insert(QStringLiteral("duplex"), duplex);
            }

            g_autofree gchar *collate = nullptr;
            if (g_variant_lookup(settings, "collate", "s", &collate)) {
                settingsMap.insert(QStringLiteral("collate"), collate);
            }

            g_autofree gchar *reverse = nullptr;
            if (g_variant_lookup(settings, "reverse", "s", &reverse)) {
                settingsMap.insert(QStringLiteral("reverse"), reverse);
            }

            g_autofree gchar *range = nullptr;
            if (g_variant_lookup(settings, "print-pages", "s", &range)) {
                settingsMap.insert(QStringLiteral("print-pages"), range);
            }

            g_autofree gchar *pageRanges = nullptr;
            if (g_variant_lookup(settings, "page-ranges", "s", &pageRanges)) {
                settingsMap.insert(QStringLiteral("page-ranges"), pageRanges);
            }

            g_autofree gchar *outputFileFormat = nullptr;
            if (g_variant_lookup(settings, "output-file-format", "s", &outputFileFormat)) {
                settingsMap.insert(QStringLiteral("output-file-format"), outputFileFormat);
            }

            g_autofree gchar *baseName = nullptr;
            if (g_variant_lookup(settings, "output-basename", "s", &baseName)) {
                settingsMap.insert(QStringLiteral("output-basename"), baseName);
            }

            g_autofree gchar *outputUri = nullptr;
            if (g_variant_lookup(settings, "output-uri", "s", &outputUri)) {
                settingsMap.insert(QStringLiteral("output-uri"), outputUri);
            }
        }

        pageSetup = g_variant_lookup_value(ret, "page-setup", G_VARIANT_TYPE_DICTIONARY);

        if (pageSetup) {
            g_autofree gchar *ppdName = nullptr;
            if (g_variant_lookup(pageSetup, "PPDName", "s", &ppdName)) {
                pageSetupMap.insert(QStringLiteral("PPDName"), ppdName);
            }

            g_autofree gchar *name = nullptr;
            if (g_variant_lookup(pageSetup, "Name", "s", &name)) {
                pageSetupMap.insert(QStringLiteral("Name"), name);
            }

            g_autofree gchar *displayName = nullptr;
            if (g_variant_lookup(pageSetup, "DisplayName", "s", &displayName)) {
                pageSetupMap.insert(QStringLiteral("DisplayName"), displayName);
            }

            gdouble width = 0;
            if (g_variant_lookup(pageSetup, "Width", "d", &width)) {
                pageSetupMap.insert(QStringLiteral("Width"), width);
            }

            gdouble height = 0;
            if (g_variant_lookup(pageSetup, "Height", "d", &height)) {
                pageSetupMap.insert(QStringLiteral("Height"), height);
            }

            gdouble marginTop = 0;
            gdouble marginBottom = 0;
            gdouble marginLeft = 0;
            gdouble marginRight = 0;
            if (g_variant_lookup(pageSetup, "MarginTop", "d", &marginTop)) {
                pageSetupMap.insert(QStringLiteral("MarginTop"), marginTop);
            }

            if (g_variant_lookup(pageSetup, "MarginBottom", "d", &marginBottom)) {
                pageSetupMap.insert(QStringLiteral("MarginBottom"), marginBottom);
            }

            if (g_variant_lookup(pageSetup, "MarginLeft", "d", &marginLeft)) {
                pageSetupMap.insert(QStringLiteral("MarginLeft"), marginLeft);
            }

            if (g_variant_lookup(pageSetup, "MarginRight", "d", &marginRight)) {
                pageSetupMap.insert(QStringLiteral("MarginRight"), marginRight);
            }

            g_autofree gchar *orientation = nullptr;
            if (g_variant_lookup(settings, "Orientation", "s", &orientation)) {
                pageSetupMap.insert(QStringLiteral("Orientation"), orientation);
            }
        }

        g_variant_lookup(ret, "token", "u", &token);

        responseData.insert(QStringLiteral("settings"), settingsMap);
        responseData.insert(QStringLiteral("page-setup"), pageSetupMap);
        responseData.insert(QStringLiteral("token"), token);
        Response response(true, error, responseData);
        portalPrivate->preparePrintResponse(response);
    } else {
        Response response(false, error);
        portalPrivate->preparePrintResponse(response);
    }
}

void PortalPrivate::printFile(const Parent &parent, const QString &title, int token, const QString &file, PrintFlags flags)
{
    xdp_portal_print_file(m_xdpPortal, parent.d_ptr->m_xdpParent, title.toUtf8().constData(), token, file.toUtf8().constData(),
                          static_cast<XdpPrintFlags>((int)flags), nullptr, printedFile, this);
}

void PortalPrivate::printedFile(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;

    bool ret = xdp_portal_print_file_finish(xdpPortal, result, &error);

    Response response(ret, error);

    portalPrivate->printFileResponse(response);
}

}
