# No Qt WebEngine, the webmail escape hatch opens the system browser

MMC renders the mail in Qt's own text browser and opens the provider's web
mail in the system browser. The Qt WebEngine module was left out because it
adds a few hundred megabytes to a build that is meant to sit on a USB stick
and to start fast, and because the webview escape hatch is a button that
opens the provider's own page, which the system browser does just as well.

Consequence: html mail is shown with the limited renderer of QTextBrowser, so
a mail that leans hard on css looks plainer than it should. The escape hatch
leaves MMC for the browser, so nothing is lost, it is one click more. When
rich rendering matters more than the size of the build, WebEngine drops into
the reading pane and nothing else has to change.
