## ADDED Requirements

### Requirement: Removing a joiner and quick messages
The host SHALL be able to remove a joined heir (told why, their resume token forgotten, the others told they left) and close the session to new heirs; every heir SHALL send quick messages from a fixed list (follow me, wait, here, help, yes, no), shown to every heir with the sender's name in their own language; a Company page in the menu SHALL offer them, and for the host removing (after a question) and closing.

#### Scenario: Removed and talking
- **WHEN** a host closes the session and a fourth heir tries to join, removes one of two joiners, and quick messages arrive and are sent from the Company page
- **THEN** the fourth is refused with the reason, the removed one is told why and cannot come back as that heir, the others see them gone, and the messages show as "<name>: Wait" in each heir's language
