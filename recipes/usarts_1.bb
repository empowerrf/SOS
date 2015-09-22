LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

COMPATIBLE_MACHINE = "(g2h)"

SRC_URI ="git://github.com/empowerrf/sos_rs485.git;tag=master;protocol=git \
"
S = "${WORKDIR}/git"

do_compile () {
    cd ${S}
    oe_runmake CC="${CC}" CXX="${CXX}"  GXX="${CXX}"
}
do_install () {
   install -d ${D}${sbindir}
   install -m 0755 ${S}/readSerialBus ${D}${sbindir}
   install -m 0755 ${S}/rs485Test ${D}${sbindir}
   install -m 0755 ${S}/rs485    ${D}${sbindir}
}

#SRCREV = "e0465a1e310b1d94c0bb7464ddf7bedfe596c9e0"
#FILES_${PN} = "/home/root ${sbindir}"

