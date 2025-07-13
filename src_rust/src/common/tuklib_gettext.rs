// Équivalent de tuklib_gettext.h - internationalisation

// Pour l'instant, pas d'internationalisation réelle
// Juste des stubs qui retournent les messages tels quels

pub fn tuklib_gettext_init(_package: &str, _localedir: &str) {
    // Stub - pas d'initialisation gettext pour l'instant
}

// Macro pour marquer les chaînes traduisibles
macro_rules! gettext {
    ($msgid:expr) => {
        $msgid
    };
}

// Macro pour marquer les chaînes sans les traduire
macro_rules! noop_gettext {
    ($msgid:expr) => {
        $msgid
    };
}

// Macro pour le pluriel
macro_rules! ngettext {
    ($msgid1:expr, $msgid2:expr, $n:expr) => {
        if $n == 1 { $msgid1 } else { $msgid2 }
    };
}

// Macro pour les chaînes word-wrapped
macro_rules! wrap_gettext {
    ($msgid:expr) => {
        $msgid
    };
}

pub(crate) use gettext;
pub(crate) use noop_gettext;
pub(crate) use ngettext;
pub(crate) use wrap_gettext; 