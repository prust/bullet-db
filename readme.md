How to implement indexes in an auto-incrementing integer database:

Say every payment has a category ID and you want an index on the category so you can quickly pull up all payments in the "Entertainment" category (ID: 18). The index will be a table where each 64 byte record corresponds to the category ID -- so Entertainment would be the 18th record. And it will contain a set of payment IDs. The very last byte of the record will contain the location of the overflow record, if there is one. And the last byte of *that* record will contain the address of another overflow record and so on.

Overflow text fields will have a similar overflow address to an overflow page. The loading API (which is async) will have an option of whether to load in associated overflow pages. And there will be an async API to load overflow pages for a particular record or page. But the data access APIs themselves will be synchronous and you will be given the option of pulling in overflow data or not.

One really cool thing about this setup is that you can load from disk *just the page from the index that has row 18 in it* (and, with the overflow flag set to true, also load any overflow pages that that page references).

* * *

LevelDB (Log-Structured-Merge Tree) -- similar to all varieties of B-Trees and Hashing and most DB theory, is based on the assumption of needing to index arbitrary values (usually strings) for fast retrieval.

However, my experience is that in good database design, this is almost never necessary. Either you normalize and assign IDs to discrete string values (for instance categories) or you want Full-Text-Search (which internally normalizes and assigns IDs to each word).

Is that true? What about searching on first_name? To get the "Frank"s, to find the one you're looking for? Or searching on last_name to find the "Flintstone"s? Hm, these could be normalized out... (an index, anyone?). It's so incredibly rare to be looking for "the F's" (b/c I remember his name started with an "F"). No, you're really searching for a discrete value. Stick it in its own table (call it an index, whatever). Or, if you're searching for a word, for instance in email subject lines, then you want a full-text search.

What I'm saying is that we should never need or want to organize things into a B-tree or LSM.

Ok, fine -- if you know the ID for "Frank". Or "Cornerstone". Or your user or account or address or what-have-you. And many times you will know the ID (or the client application will know the ID). But sometimes you won't.

And that's ok. Because the index will give you the ID. And the index will be small (strings-to-IDs). But will it be sorted? Or structured in a B-Tree? You can't structure an index over first-names the same way that you would structure an integer-to-integer index, with record 0 being for ID 0. Instead, you have to use a hashing algorithm and put all strings with the same hash value on the same row, with an overflow. Yes, let's do that. Ok, so there is a place for hashing -- but we don't need to deal with B-Trees or bubble sorting or any of that hoo-ha.

* * *

For starters, we should probably go with 8k pages b/c that's what Buffers are (but OS pages are 4k?!). Regardless, we need to fully "use" however many bytes are allotted for IDs (an 8k page could contain 128 64-byte rows).

Along with the schema, the user will need to specify the number bytes per ID (the "address size", thus implying the max number of records for the table. If we do that (and we should), then we really do need to "fill up" the ID space. It shouldn't force pages to be a certain length, it should just mean that -- based on record size -- certain bits are used to identify the page and other bits are used to identify the record within the page.

Assuming that we are able to "fill up" the ID space and use the appropriate bits, the user should be able to just specify the max number of rows, choosing from "1 byte (max 256 records)", "2 bytes (max 65,535 records)", "4 bytes (max 4 trillion records)"

* * *

Unrelated: in our metadata, we need to record how many records there are in a table, so we know where to poke a new record and when to stop iterating.

Booleans take up an entire byte, even though they only need a bit.

The deleted flag takes up an entire byte, but we reserve the other 7 bytes for other admin-type use.

Phases 1-4 are necessary for a nice, working, usable system. Phases 5-6 are necessary for robust-ness.

Cluster support is necessary/important only if your app is running in a cluster. If you're running a single node app on a single thread, then this embedded approach will be much faster than a cluster or TCP approach due to inter-process communication overhead. If you're running on Heroku or another PaaS, then yeah, you may want to run this in a cluster -- but it depends on demand. If demand isn't that high then it may be enough to simply spin this up as another dyno that your web dynos talk to. The way that Heroku forces separation of responsibilities into totally isolated dynos and add-ons is cool, but there is -- at the very least -- an IPC performance penalty. More likely a TCP performance penalty. BulletDB primary use-case is embedded use, to eliminate that performance penalty. So it doesn't work well with PaaS. Unless someone writes a TCP wrapper and it's used that way. So really there are 2 things to consider: sharding to multiple cores and using IPC, or sharding to multiple machines and using TCP. Either is above the scope, BulletDB could be a building block for such things. But it should do really well in desktop/portable apps and in simple, single-process servers.

Wait a minute... what about using S3? Reading and persisting to S3 or some other storage medium? Backups to S3? This is how all the redis providers work! And it could be embedded in-process. That would be great! *That's* how to fit in with the PaaS world. And embed your DB. Yes! So, how do you coordinate between multiple instances? In order to have the uptime, etc, you can't shard -- they all need to handle everything, right? But you could shard the saving... interesting. Some kind of eventual-consistency thing. It'd be nice if the dynos could talk with each-other and keep each-other up-to-date directly, without needing to go through some other server/service. And back up to S3 on an interval. And load from S3 on startup. Yes! This would allow your PaaS dynos to be super-fast; lightning-fast. But you would still need something slow, over TCP, akin to Redis, keeping them consistent with each-other in the background. And you need some third-party registry or service or something to let them find out about each-other. The right way would be to have a Heroku Add-On (instead of S3) that contains the official data and communicates between instances. Any of the add-ons could work, but it should be something fast, preferably with push notification on changes? Probably Redis. You could use the same on nodejitsu. And cache the data locally on the server via BulletDB for super-fast response times, but query it async from Redis if/when necessary.

## Phase 1:

* Schema definition
* Foreign Key definitions are necessary so we can allocate the appropriate number of bytes
* Loading and saving data to/from buffer to objects (or selecting a single column to an flat array)
  - You can specify a flag for it to return `null` for deleted records, which will give you the record IDs
  - Userland: specify/configure for it to put the record ID on an attr
  - Userland: save(), which will perform insert() or update(), depending on if ID is null
  - Userland: specify/configure for it to put the table name on an attr
  - Userland: filtering
  - Userland: joins to related tables, etc
  - Core API has none of the userland stuff:
    - `update(table, id, record)`
    - `insert(table, record)`
    - `select(table[, from, to])` (`from` and `to` work the same as in `Array.slice()`, supporting negative numbers; the async load API should be similar)
    - `fetch(table, id)`
    - `delete(table, id)`
  - Properties not in the schema are ignored
* Background/async persistence to disk of dirty records
* Async loading of tables and pages (top X records or page for X ID or set of IDs)
* Iterating records
* Inserting new record and getting the new ID back
* Delete/undelete support: there should be a byte for admin use, one bit of which is the delete flag
  * with a flag to actually "scrub" a record, otherwise you can "undelete" before ID-reuse
* Browser support (or Phase 1b?)

## Phase 2:

* Indexes on IDs
  - an index is really an ID to an (overflowing) array of IDs
  - also can be used for many-to-many tables
    - call it an "overflowing array" record type
    - it can be used for arbitrary purposes (uses less space than one-record-per-item, fit a lot more in a 64-byte cache line, more denormed... but all items need to be the same size/type.)
    - it can be specified as a foreign key (so it will be cleaned on a cascading delete by the built-in key-reuse deal -- or by any cascading delete)

* Index Overflow (index overflow pages should be index-specific -- IOW, one set of overflow pages for one index)

## Phase 3:

* Text Overflow (text overflow pages should be table-specific, but not column specific -- IOW, one set of overflow pages for each table)
* Explicit unloading of tables/pages

## Phase 4:

* Indexes of strings and other arbitrary values via hashing
  - this could almost be userland, except it requires a unique data structure
  - it's not an "overflowing array" b/c multiple arbitrary values can land on the same hash, so it's instead an array of IDs for each value

## Phase 5:

* ID-Reuse is an explicit process (not auto) and might be slow (walking all foreign keys without indexes). Anything fancier (store datetime stamps, rolling re-use, implicit/auto re-use, etc) would be a user extension.
* Array of deleted records is used to know where to insert new records until it is used up (could use the "overflowing array" record type).

## Phase 6:

* Explicit backup command to verify that the backup happens without partial reads/writes
  - Simplest implementation: (faster, but blocks all writes to all pages)
    - block all incoming writes until all file operations have finished going to disk
    - copy the entire file
    - once it's done copying, unblock the incoming writes
  - Better implementation: (slower, but only blocks writes to one pages at a time)
    - Walk all pages in order, queueing the writes in order, blocking each page as you go (or waiting for file operations to complete) -- a bunch of pages at a time. 

## Other Ideas:

* Port to C (with libuv) and write bindings for node
* Automatic unloading (or marking to be unloaded) of pages that haven't been used for a certain time window
  - this should be userland
* Do we care at all about transaction support? Not really, there's record-level isolation due to node's single-threadedness and sharding out to a node cluster
* Use node cluster to take advantage of multi-core CPUs, by sharding the data out to different processes
  - Each process is responsible for different row numbers, they each have all the files open
  - They would use locks to deal with the metadata.
  - This really should be a separate layer, a separate project, on top of this one, that uses this one.

## Tiny Core

Indexes and Hashing do not belong in core and can be in userland. They might be slightly slower if they're not in core, but it's something to measure and think really hard about -- *afterwards*. Backup also should be a separate module. Each of these will require certain generic hooks or structures in core -- and that's a good thing -- but can be implemented mostly or entirely outside of the core module.

Delete -- the flag and the re-use of IDs -- would be two separate modules. Should it be a separate *file* or a separate *module*? It would be really interesting to see if we could make it a separate module -- it would require some deep hooks in places, but perhaps that's for the best. You could specify which bit/byte you want the delete module to use... and it would wrap the various APIs...

Rather than seeing them or seeing a lot of these things as inheritance, what if we saw them as composition? What would the low-level API look like that these other modules are using?

Text overflow could be a separate module too -- very interesting. If we pulled everything possible out of core, what would be left? What would the API look like? What would it have to look like in order to support all the modules and extensions? Would there be a chain of functions, all wrapping each-other? Or registered extensions? Or extensions that all listen for events? I'm not sure.

The function calls will have some overhead, but thats micro-optimization -- part of me thinks it may be wise to draw a bigger circle around 1k lines worth of functionality and call that a module -- but the rest of me says: No. Do the node thing, the unix thing, and make a flexible, powerful core that can handle all the other stuff (indexes, hashing, delete, ID re-use, text-overflow, etc) as extensions. And see what is left over. It should be amazing and should avoid getting a billion pull requests as every one wants to add all kinds of odd functionality to it.

You could even split out the schema/foreign-key consideration out to a separate layer and -- at the bottom layer -- just have something that manages records of a particular size, organized into pages, with record IDs (of a certain size). All of it binary/buffer based. And "record"/object style could use it and so could the "overflowing array" style, and the "overflowing hash" style.

* * *

Most developers want a document-oriented database. But they want performance. What they (probably) really want is a blob with certain columns materialized and indexed -- like IndexedDB. We should think about implementing the IndexedDB API -- or part of it, anyway, on top of this. It would be great to provide a document-oriented API on top of this. And a string-key like API via hashing? We could -- as an optional extension -- might be useful for migrations, but it does go against the grain of what we're doing here. But it should be fine. Like an index... But not recommended.

It would be a really nice feature to allow the data to be saved in separate files -- one per table. Not sure if the metadata should go at the top of the file or in a separate index... probably the top of the file. That way each file -- each table -- can be easily moved around. One cool thing about that is that it makes the data a lot more discoverable/accessible. Sure it's binary, but *they're* telling us the binary format, the schema. Stick it in any binary file editor, split on 64-byte records or 128-byte records, and you've got tabular data. Very cool.

Free your data. That's important. Yeah, it may be binary, but it's in an easily-readable, accessible and friendly format. Overflow pages would be mixed in... yeah I guess they should be in a separate file too. So yeah, that's annoying b/c sometimes you really want a single file. There should be a utility to split between a single and separate. Maybe just keep it single and let people use the tool to see the contents.

* * *

Build with small-and-sharp tiny things: one thing that abstracts separate pages (buffers) or fixed-size records and presents it as referenceable with a single record_id. Another thing that flushes dirty pages to disk in an async way (probably using an async queue or something).

* * *

Consider compaction/vacuum (which will shuffle virtually ALL ids and update ALL FKs) as an alternative to re-using deleted keys. The act of compacting will have some performance impact, but LIMIT/OFFSET and the order of things will still be meaningful and recent items will be on the same page instead of spread out over a bunch of pages. There are pros and cons both ways -- both can be options as extensions/modules, not part of the core. But for the email client, I would need compaction -- at least for emails -- b/c I want to show the most recent emails and because the disk access will be less random (it's more likely that old items will be grabbed together and that new items will be grabbed together -- if new items are randomly interspersed among old items, you lose that). Yikes/yuck -- what if I just now receive an email from a day ago? Emails will be ordered in the order in which I receive them, not the date. I have two options: (A) index by date (B) introduce a bubble sort/shuffle sort of thing -- or even manually re-assign. The Index adds pointers and steps and indirection (I would need to pick an appropriate hashing function so all recent dates don't end up in the same bucket!). Or I could even have a "base day" (the day the user started the program) and base the numbers off of that base day, and do an array of email IDs for that day. That wouldn't be so bad. And it's basically hashing -- really, it's a perfect hash (except for negative dates -- it would make sense to partition by year, maybe by month as well). So yeah, those are the options. It seems like the "right" solution is to display them based on date -- and that's often the case, to display things in order, based on something other than an ID. But if that something is a lookup to an ID -- and especially if the IDs tend to be on the same pages -- then you're doing pretty good. You can manipulate your hashing algorithm so that the ones you want the most or the best will appear on the same page and just load that page and just load the IDs referenced on that page. In this case, the dates keep marching forward, if the hash is similar (one hash per date, marching forward), then you just need to grab the last page (and maybe the 2nd to the last also), and load the IDs referenced by the top ones there.

http://www.reddit.com/r/programming/comments/1mfnzo/sophia_new_embeddable_keyvalue_database_designed/
