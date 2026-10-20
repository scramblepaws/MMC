# Mail is only fetched when the user asks

There is no background refresh in MMC. Mail arrives when the user presses
Refresh or Refresh all. With a hundred mailboxes, a poll loop would mean a
hundred connections or a hundred requests per cycle, which the providers
answer with throttling, and the user would have to keep the program running
all day for it to be worth anything.

Consequence: MMC has no notifications and no unread badge that updates on
its own. The unread count is the one from the last refresh. When the
polling is wanted later, it belongs behind one setting and one scheduler,
not scattered over the code.
