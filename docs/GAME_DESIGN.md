# Corner Shop Quest — Game Design (draft v0.6)

> Working title. Genre: **2D top-down adventure RPG + family business**.
> Status: **draft v0.6** - Phases 1-5 (Chapters 1 to 4, two generations) built; see the status
> sections. Items marked *(provisional)* are defaults awaiting a decision — see
> [Open decisions](#open-decisions).

## Vision

Your family's small corner shop is struggling since a big store opened in town. You (about 10)
and your brother or sister decide to help. You explore the neighbourhood, then the town, nearby
villages and the city; meet people with real-life problems; make friends who join your
adventures; and grow the shop from a pushcart into a store and beyond.

Every quest is a real-life situation — a neighbour being scammed, a friend left out, a supplier
offering a too-good deal, a parent stressed about bills. Children come away understanding how
money, work and people actually work, learned through consequences they experienced, not text
they were told.

## Pillars

1. **Adventure** — explore, meet people, unlock new places.
2. **Build from scratch** — pushcart → stall → store → chain; visible growth.
3. **Real-life economics** — earning, pricing, saving, borrowing, giving; time is scarce.
4. **People** — family, sibling, friends; understanding why others act as they do.
5. **Unlimited** — new regions, an ever-growing business, later a new generation.
6. **Healthy engagement** — compelling without manipulation.

## Design rules

- **No combat.** Conflicts are social and economic; challenges are solved with life skills.
- **Every system must produce a choice.** Compress time; keep only the decisions.
- **Teach through consequences, not text.** A short reflection afterwards makes lessons stick.
- **Delayed consequences.** Choices can resurface days or chapters later.
- **Failure is recoverable.** There is always a path back.
- **No single score.** Success is multi-dimensional (Life Wheel).

## Core loops

| Scale | Loop |
|---|---|
| Moment | Walk the world, talk to people, inspect things, small interactions |
| Day | Morning: buy stock and set prices → choose: run the shop yourself, or leave the sibling/a friend in charge and go adventuring (opportunity cost) → evening family dinner (events, family budget) → **sleep = autosave and natural stopping point** |
| Quest | Meet someone with a real-life problem → choices and encounters → consequences (relationships, reputation, money) → rewards (a friend joins, a new supplier, a new area) |
| Chapter | A story goal (e.g. "earn enough by the festival to rent a stall") → shop tier up → new region opens |
| Generation *(later)* | Hand the business to your child; their start reflects your choices |

## Story outline (chapter 1 sketch)

1. **The empty shop** — fewer customers since MegaMart opened; Mom and Dad worry about rent.
2. **The pushcart** — you and your sibling start selling snacks at the park with savings from your
   piggy bank. First lessons: cost price vs selling price, profit, spoilage.
3. **First friend joins** — helping a new kid who is being left out; they join your party.
4. **A too-good offer** — a stranger offers cheap stock "today only" (scam awareness).
5. **Festival day** — chapter goal: enough saved to rent a proper market stall.

## Party and life skills

Friends and your sibling join the party. Instead of combat stats, everyone has **life skills**
that grow by being used:

| Skill | Used for |
|---|---|
| Money sense | Pricing, budgeting, spotting bad deals |
| Communication | Persuading, negotiating, explaining |
| Empathy | Calming people, understanding hidden problems |
| Creativity | New products, displays, solving puzzles differently |
| Hard work | Long shop days, physical tasks, stamina |

Each companion has a personality, a personal story arc, and opinions about your choices — they
may disagree, leave for a while, or come back.

## Encounters (replace battles)

Turn-based challenges where the party uses life skills: **negotiating** with a supplier,
**calming** an upset customer, **helping** a friend through a problem, **convincing** the
council to allow a stall. Actions like *Listen*, *Explain*, *Offer a deal*, *Ask a friend for
help*. The other side has a mood and hidden needs; understanding them (Empathy, "Their Side")
unlocks the best outcome. Every encounter can end in more than one acceptable way.

## Shop system

- **Tiers:** pushcart → market stall → corner shop → bigger store → chain (new regions).
- **Stock:** buy from suppliers (price, quality, reliability); some goods spoil.
- **Pricing:** customers have budgets and preferences; too high loses sales, too low loses money.
- **Reputation:** honesty, quality and friendliness bring repeat customers.
- **Costs:** rent, wages, repairs, unexpected expenses.
- **Staff:** hire friends (wages, fairness, friendship vs business tension).
- **Bank:** savings with visible interest (a growing tree), loans showing "total you'll pay back".
- **Family link:** shop profits help the family budget; tough weeks show at the dinner table.

## People system

- Every character has **personality traits, likes/dislikes, mood, memory of what you did, and
  their own goals and problems**.
- Relationships have three dimensions: **Trust, Fun, Respect**.
- **Sibling:** your permanent partner — rivalry and loyalty arcs, shared credit and blame,
  co-owner of the business.
- **Friends:** made through quests and shared interests, kept by showing up, drift if neglected;
  conflicts repaired with a sincere apology; peer-pressure moments.
- **Family:** parents with visible struggles; grandparents with wisdom (and old shop stories).
- **Signature mechanic — "Their Side":** after a conflict, play a short scene as the other person
  and see why they acted that way.

## Life Wheel (success)

Six dimensions: **Money, Health, Happiness, Relationships, Knowledge, Community**. No single
score. At each chapter end the **Shop Journal** shows the wheel's shape and the three moments
that changed it most. Extremes have visible costs (rich but friendless, popular but broke).

## Healthy engagement

- **Use:** attachment to companions, visible shop and town growth, goal ladder (today / chapter /
  long term), curiosity about new regions, surprise events, a memory album, mastery.
- **Never:** loot boxes, real-money purchases, streak penalties, FOMO timers, energy-wait gates,
  pushy notifications.
- **Built in:** sleep as a stopping point, optional parent time limit, journal reflection.

## Content: quests as data

Quests, dialogue and scenarios live in JSON under `assets/quests/`, not in C++, so content scales
without code and non-programmers can contribute.

```json
{
  "id": "too_good_offer",
  "when": { "chapter": 1, "flag": "pushcart_open" },
  "speaker": "stranger_van",
  "text": "Biscuits at half price! Today only, cash up front, no receipts.",
  "choices": [
    { "label": "Buy 50 packs", "effects": { "money": -40 }, "later": "biscuits_expired" },
    { "label": "Ask to see the expiry date", "requires": { "money_sense": 2 }, "then": "stranger_leaves" },
    { "label": "Ask Grandpa first", "effects": { "grandpa.trust": 5 }, "then": "grandpa_scam_story" }
  ]
}
```

- `when` — trigger conditions (chapter, flags, stats, relationships).
- `requires` — skill or relationship needed for the choice.
- `effects` — immediate changes.
- `then` — follow-up scene right away.
- `later` — sets a flag a future event can trigger on (delayed consequence).

## Technical plan

Built on the existing engine (`engine/`: Application, fixed-timestep loop, scene stack).

| System | Approach |
|---|---|
| World | 2D top-down tile maps authored in the Tiled editor; areas connected by doors/paths |
| Player & NPCs | Grid-aligned movement, collision, NPC schedules |
| Dialogue / quests | JSON-driven event engine: conditions, effects, flags, follow-ups |
| Encounters | Turn-based scene overlay using party life skills |
| Shop | Simulation of customers, stock, prices, money; runs in the fixed-timestep update |
| Time | Clock + calendar: days, weeks, festivals, chapters |
| UI | Dialogue boxes, shop/budget screens, journal — scene overlays |
| Data | nlohmann-json (MSYS2 package) |
| Save / load | Autosave on sleep; whole game state to JSON |
| "AAA" | Polish: art direction, animation, audio, game feel, accessibility, localization |

## Roadmap

- **Phase 0 — Design:** settle open decisions; list ~30 life lessons; write chapter 1 quests on
  paper.
- **Phase 1 — Vertical slice "The pushcart":** home, shop, park, one street; player + sibling,
  parents, grandparent, two friends; walking, talking, dialogue choices; pushcart day loop (buy
  stock, set price, sell to customers); 3 quests including the scam; one encounter; one
  "Their Side" scene; save/load. Test with real kids.
- **Phase 2:** full chapter 1, market stall tier, more friends, encounter system complete.
- **Phase 3:** town and villages, corner shop tier, staff, bank and loans.
- **Phase 4:** city region, chain tier, generations, polish, audio, localization.

## Phase 1 status

Built and playable (see README for controls):

- Town map: home, corner shop, MegaMart, Lin's Wholesale, park with pushcart spot.
- Characters: Mom, Dad, Grandpa, Maya (follows you), Leo (joins as a follower), Priya,
  Mrs. Lin, Smiley Steve. Gold "!" markers show who has something new to say.
- Day cycle: 08:00-20:00 clock, hot/mild weather, dinner events, end-of-day summary, autosave.
- Business loop: buy stock at cost price, set selling prices, simulated customers, revenue /
  cost / profit breakdown with a lesson, spoilage, reputation, "let Maya run it" option.
- Quests: The Old Pushcart (rent by Day 7), The New Kid (encounter), Too Good To Be True (scam).
- Encounter: "The New Kid" - listen first, then invite; order matters.
- "Their Side": Maya's side of the stolen-coins conflict, plus repair through apology.
- Journal: quests, bag, skills, people (trust / fun / respect).
- Consequence toasts ("Maya trust +15", "Quest complete: ...").

## Phase 2 status - Chapter 1 complete

Built on top of Phase 1:

- **Second area: the market square** (fountain, stalls, bunting), reached by walking east. The
  engine now supports any number of areas connected by exits, each a JSON map.
- **Market stall tier:** rent a stall from Mr. Okafor (60 coins, or 45 if you negotiate well).
  The stall brings ~1.7x the customers of the pushcart but costs a 5 coin fee each time you
  open - a fixed cost the results screen shows separately.
- **New products:** Mom's Cookies (her recipe: ingredients 2, sells for 5 - "adding value") and
  Sandwiches (unlocked with the stall). Items can be unlocked or withdrawn by story flags.
- **Encounters complete:** data-driven mood labels, win/lose texts, action requirements and
  costs. Three new ones: negotiating the stall price (Mr. Okafor), calming an angry customer
  (Mrs. Grumble), and turning a rival into a partner (Kofi - you stop selling apples, he sends
  thirsty customers to you).
- **More friends:** Priya can join the team; companions give data-driven bonuses at the stall.
- **Festival Day (Day 14):** the market is decorated and full (2.5x customers at the stall), the
  park is empty (0.4x at the pushcart). That evening Chapter 1 ends.
- **Life Wheel:** Money, Health, Happiness, Relationships, Knowledge, Community - shown in the
  journal's Life tab and on the chapter-end screen with the "moments that mattered".
- **Health:** an early night restores health, staying out until dark costs it; when tired you
  walk slower.

## Phase 3 status - Chapter 2 "The Corner Shop" (Days 15-28)

- **Partners with Dad:** the corner shop becomes a venue you run (door menu). Upgrades are
  investments: fridge (80), sign (30), shelves (40). Each adds customers; the fridge keeps
  lemonade 3 days, sandwiches 2 days, and unlocks Iced Milkshakes.
- **Town Bank (Ms. Adeyemi, market square):** savings earn 5% every 7 days (a savings tree grows
  coin fruit); loans are repaid in weekly instalments on bank day, with a late fee if you can't
  pay; early repayment allowed. A loan-interview encounter rewards a repayment plan with 10%
  instead of 25% interest. You can also refuse and save up.
- **Staff and wages:** Maya, Leo and Priya can each run one venue a day for the team wage. The
  pay talk sets it: 1 coin (staff sell 25% less, trust falls), 3 (fair), or 5 (staff sell 15%
  more, but it costs). Results show the wage as a cost line.
- **Price war:** MegaMart sells lemonade for 2 coins for a week - customers value yours less and
  the shop loses visitors - unless you have the fridge. Lesson: compete on quality, not price.
- **Green Hill Farm (first village, west of town):** Farmer Ama explains the supply chain
  (farm -> wholesaler -> shop); the Farm Shop sells strawberries and honey you can't get in town;
  helping with the harvest trades two hours of hard work for stock.
- **Engine/data:** suppliers, item value rules, fridge shelf life, staff profiles, venue
  bonuses (including negative ones), quests tagged by chapter (the HUD tracks the current
  chapter first), a generic chapter-end screen and chapter titles in data.

## Phase 4 status - Chapter 3 "Two Shops" (Days 29-42) and polish

- **Riverside City:** a new area reached by bus (2 coins and an hour each way - or walk home free
  in three hours). City Market, Cafe Lucia, MegaMart City (high prices where the small shops
  closed), a park and a busker who talks about saving on good days.
- **Bulk buying:** the City Market sells at a lower price per item when you buy 20 or more; city
  pretzels are only sold there. Lesson: economies of scale - if you can sell it all in time.
- **Chain tier:** rent a second shop from Mrs. Osei (40 deposit, 20 a week - 15 if you negotiate a
  year's tenancy). Rent comes out on bank day from your pocket, then savings; late rent costs
  reputation and customers.
- **Managers:** choose Priya, Leo or Grandpa to run the city shop. The manager sells there every
  evening from your shared stock for the team wage - unless they're busy, upset, or there's no
  stock. Choosing one friend over another leads to a "Hurt Feelings" encounter: listen first,
  then give them a role of their own (Head of Design / Head of Sales).
- **Young Business Fair (Day 42):** the chapter ends with a ribbon - or, without a second shop,
  a lesson that knowing when not to grow is smart too.
- **Audio:** a small synthesizer in the engine builds sound effects (clicks, coins, success,
  failure, purchases) and a gentle music loop from notes at start-up - no audio files needed.
- **Settings:** music, sound effects and fullscreen in the pause menu (M toggles music), saved
  to settings.json.

## Phase 5 status - Generations: Chapter 4 "Roots & Wings"

- **The hand-over:** after Chapter 3 the player chooses when to jump twenty years ahead (or plays
  another week first). A legacy screen shows the Life Wheel of the life lived, what became of the
  business and every friend, and how the child starts.
- **Inheritance rules:** the child gets a fifth of the family fund as pocket money (10-40) and a
  quarter in a savings account, half the family reputation, fresh skills plus one family talent
  (the parent's best skill). Debts stay with the parent who borrowed.
- **The town remembers:** world history (story flags, seen events, memories) carries over, so the
  grown-ups tell different stories depending on what you did - Aunt Maya remembers whether you
  listened to her side, Kofi whether you made peace, Leo and Priya whether you invited them in.
- **Second generation:** your child (new look), cousin Nia (on the team, can work a stand),
  Mom or Dad (the grown-up first child - the player chooses what to call them), Grandma, Grandpa
  at the shop, Great-Grandpa at the farm, and Clover the cow.
- **Giving back:** for the town's 100th festival (Day 14) the child chooses a community project
  (playground, library books or trees) and must earn and then choose to give 40 coins.
- **Engine/data:** generation tags on characters, quests and triggers; display-name overrides;
  a family-generation-aware Life Wheel and People page.
- **Animation:** walking legs and arms, characters face the way they move, people turn towards
  you, and everyone blinks.

## Roadmap after Phase 5

- **Localization:** the text lives in JSON, so translations can be added per language - pick the
  language(s) first.
- **Art and audio assets:** replace the code-drawn placeholders with drawn sprites and recorded
  sound once assets are chosen.
- **More chapters and generations,** and **testing with real kids** to tune difficulty and text.

## Open decisions

| # | Decision | Current default |
|---|---|---|
| 1 | Target age | 9–13 *(provisional)* |
| 2 | Setting / culture (money, school, family structure, festivals) | Fictional, culture-neutral town *(provisional)* |
| 3 | Hardship tone | Cozy with gentle real setbacks, always recoverable *(provisional)* |
| 4 | Combat | None — encounters use life skills *(provisional)* |
| 5 | Multiplayer | Single-player only *(provisional)* |
| 6 | Title | "Corner Shop Quest" *(working title)* |
