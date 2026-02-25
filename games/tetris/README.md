# Tetris

| Feature                          | How to test                                    |
| -------------------------------- | ---------------------------------------------- |
| Window opens (both backends)     | Run both binaries                              |
| Field boundary visible           | Walls shown on all sides                       |
| Piece spawns at top center       | Watch where each piece appears                 |
| Left/right movement              | One tap = one move; holding doesn't slide      |
| Rotation                         | Up/Z rotates; blocked near walls stays put     |
| Soft drop                        | Hold down — falls faster                       |
| Piece locks when it can't fall   | Drop a piece; watch it freeze                  |
| Locked cells stay                | Stack multiple pieces; earlier ones don't move |
| Line detection fires             | Fill a row; it turns white                     |
| Flash animation                  | White row visible for ~400ms                   |
| Row collapses                    | After flash, row disappears; pieces above drop |
| Score increases                  | +25 per piece; +200/400/800/1600 per line(s)   |
| Level shows correctly            | Increases every 50 pieces                      |
| Speed increases at higher levels | Drops feel faster                              |
| Next-piece preview               | Sidebar preview matches what actually spawns   |
| Game over triggers               | Fill field to top; overlay appears             |
| Game over shows score            | Final score in the overlay                     |
| R restarts                       | Board clears, score resets, pieces start again |
| Escape quits                     | Window closes, no crash                        |
| Valgrind clean                   | `definitely lost: 0`                           |
