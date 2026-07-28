#!/bin/sh

#  Snapmaker_Orca gettext
#  Created by SoftFever on 27/5/23.
#

# Check for --full argument
FULL_MODE=false
for arg in "$@"
do
    if [ "$arg" = "--full" ]; then
        FULL_MODE=true
    fi
done

if $FULL_MODE; then
    xgettext --keyword=L --keyword=_L --keyword=_u8L --keyword=L_CONTEXT:1,2c --keyword=_L_PLURAL:1,2 --add-comments=TRN --from-code=UTF-8 --no-location --debug --boost -f ./localization/i18n/list.txt -o ./localization/i18n/Snapmaker_Orca.pot
    python3 scripts/HintsToPot.py ./resources ./localization/i18n
fi


echo "$0: working dir = $PWD"

# FOS: the app loads <SLIC3R_APP_KEY>.mo at runtime - GUI_App.cpp does
# AddCatalog(SLIC3R_APP_KEY) - and version.inc sets SLIC3R_APP_KEY=FOrcaSlicer.
# Upstream this script emitted Snapmaker_Orca.mo, a name the renamed app never
# looks for, so the mac build shipped catalogs with no way to load them and the
# language list came up empty. The Windows path was already fixed in CMake
# (gettext_po_to_mo -> ${SLIC3R_APP_KEY}.mo) but it runs tools/msgfmt.exe and
# cannot serve macOS. Derive the key from version.inc so the two cannot drift.
APP_KEY=$(sed -n 's/^set(SLIC3R_APP_KEY "\(.*\)").*/\1/p' version.inc)
[ -n "$APP_KEY" ] || APP_KEY="FOrcaSlicer"
echo "$0: APP_KEY = $APP_KEY (mo files will be <lang>/$APP_KEY.mo)"

pot_file="./localization/i18n/Snapmaker_Orca.pot"
for dir in ./localization/i18n/*/
do
    dir=${dir%*/}      # remove the trailing "/"
    lang=${dir##*/}    # extract the language identifier

    if [ -f "$dir/Snapmaker_Orca_${lang}.po" ]; then
        if $FULL_MODE; then
            msgmerge -N -o "$dir/Snapmaker_Orca_${lang}.po" "$dir/Snapmaker_Orca_${lang}.po" "$pot_file"
        fi
        mkdir -p "resources/i18n/${lang}"
        if ! msgfmt --check-format -o "resources/i18n/${lang}/${APP_KEY}.mo" "$dir/Snapmaker_Orca_${lang}.po"; then
            echo "Error encountered with msgfmt command for language ${lang}."
            exit 1  # Exit the script with an error status
        fi
    fi
done
