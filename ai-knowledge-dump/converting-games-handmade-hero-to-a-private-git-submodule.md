# Converting `./games/handmade-hero` to a Private Git Submodule

## Overview

This guide walks you through extracting `./games/handmade-hero` into a private repository while keeping the engine public, and establishing workflows for development, contribution, and future game submodules.

---

## Step 1: Create the Private Repository

### 1.1 Extract History (Optional but Recommended)

If you want to preserve git history for the `handmade-hero` directory:

```bash
# Clone your repo to a temporary location
git clone --no-local . ../handmade-hero-extracted

cd ../handmade-hero-extracted

# Use git-filter-repo (install via pip install git-filter-repo)
git filter-repo --subdirectory-filter games/handmade-hero
```

### 1.2 Create Private Repo on GitHub/GitLab

```bash
# Create new private repo (using GitHub CLI)
gh repo create your-username/handmade-hero --private

# Or manually create on GitHub, then:
cd ../handmade-hero-extracted
git remote set-url origin git@github.com:your-username/handmade-hero.git
git push -u origin main
```

---

## Step 2: Remove Directory and Add as Submodule

### 2.1 Remove from Main Repository

```bash
cd /path/to/your/main/project

# Remove the directory from git tracking (keeps local files temporarily)
git rm -r --cached games/handmade-hero

# Actually remove the directory
rm -rf games/handmade-hero

# Commit the removal
git commit -m "chore: remove handmade-hero in preparation for submodule conversion"
```

### 2.2 Add as Submodule

```bash
# Add the private repo as a submodule
git submodule add git@github.com:your-username/handmade-hero.git games/handmade-hero

# Commit the submodule addition
git commit -m "chore: add handmade-hero as private submodule"

git push
```

---

## Step 3: Update Build System

### 3.1 Modify `build-common.sh`

```bash:engine/build-common.sh
#!/bin/bash

# ═══════════════════════════════════════════════════════════════════════════
# SUBMODULE VALIDATION
# ═══════════════════════════════════════════════════════════════════════════

de100_check_submodule() {
    local submodule_path="$1"
    local submodule_name="$(basename "$submodule_path")"

    if [[ ! -d "$submodule_path/.git" && ! -f "$submodule_path/.git" ]]; then
        echo "═══════════════════════════════════════════════════════════════════"
        echo "ERROR: Submodule '$submodule_name' not initialized!"
        echo "═══════════════════════════════════════════════════════════════════"
        echo ""
        echo "This game is stored in a private submodule."
        echo ""
        echo "If you have access, run:"
        echo "  git submodule update --init --recursive games/$submodule_name"
        echo ""
        echo "If you don't have access:"
        echo "  - Contact the repository owner for access"
        echo "  - Or create your own game in games/your-game-name/"
        echo "    (see games/README.md for instructions)"
        echo ""
        echo "═══════════════════════════════════════════════════════════════════"
        return 1
    fi

    # Check if submodule is empty (initialized but not cloned)
    if [[ ! -f "$submodule_path/src/main.c" ]]; then
        echo "═══════════════════════════════════════════════════════════════════"
        echo "ERROR: Submodule '$submodule_name' appears empty!"
        echo "═══════════════════════════════════════════════════════════════════"
        echo ""
        echo "The submodule directory exists but content is missing."
        echo ""
        echo "Try running:"
        echo "  git submodule update --init --recursive games/$submodule_name"
        echo ""
        echo "If that fails, you may not have access to the private repository."
        echo ""
        echo "═══════════════════════════════════════════════════════════════════"
        return 1
    fi

    return 0
}

de100_check_all_game_submodules() {
    local games_dir="$1"
    local has_error=0

    # Check .gitmodules for registered submodules in games/
    if [[ -f ".gitmodules" ]]; then
        while IFS= read -r submodule_path; do
            if [[ "$submodule_path" == games/* ]]; then
                if ! de100_check_submodule "$submodule_path"; then
                    has_error=1
                fi
            fi
        done < <(git config --file .gitmodules --get-regexp path | awk '{print $2}')
    fi

    return $has_error
}
```

### 3.2 Update Game Build Script

```bash:games/handmade-hero/build-dev.sh
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common build utilities
source "$PROJECT_ROOT/engine/build-common.sh"

# ═══════════════════════════════════════════════════════════════════════════
# SUBMODULE CHECK
# ═══════════════════════════════════════════════════════════════════════════

# This check runs even if someone copies the build script
if ! de100_check_submodule "$SCRIPT_DIR"; then
    exit 1
fi

# ═══════════════════════════════════════════════════════════════════════════
# REST OF BUILD SCRIPT
# ═══════════════════════════════════════════════════════════════════════════

# ... existing build logic ...
```

---

## Step 4: Create Documentation

### 4.1 Root README Update

Add this section to your main `README.md`:

````markdown:README.md
## 🎮 Games Directory Structure

This engine supports multiple games. Some games are stored as **private submodules**.

### Public vs Private Games

| Directory | Status | Access |
|-----------|--------|--------|
| `games/handmade-hero/` | 🔒 Private Submodule | Requires explicit access |
| `games/example-game/` | 📖 Public | Available to all |

### For Contributors

#### If you have submodule access:

```bash
# Clone with submodules
git clone --recursive git@github.com:your-username/de100-engine.git

# Or if already cloned:
git submodule update --init --recursive
````

#### If you don't have submodule access:

You can still:

1. Build and use the engine
2. Create your own game in `games/your-game-name/`
3. Contribute to engine code

See [games/README.md](games/README.md) for creating a new game.

````

### 4.2 Games Directory README

```markdown:games/README.md
# Games Directory

This directory contains games built with the DE100 engine.

## Directory Structure

````

games/
├── README.md # This file
├── .gitkeep # Ensures directory exists
├── handmade-hero/ # 🔒 Private submodule
│ └── (requires access)
└── your-game-name/ # Create your own!
├── build-dev.sh
└── src/
├── main.c
├── main.h
└── inputs.h

````

## Creating a New Game

### 1. Create Directory Structure

```bash
mkdir -p games/my-game/src
````

### 2. Create Minimal Files

```c:games/my-game/src/main.c
#include "../../../engine/_common/base.h"
#include "../../../engine/game/base.h"

// Your game code here
GAME_UPDATE_AND_RENDER(game_update_and_render) {
    // Render a test pattern
    render_weird_gradient(buffer, 0, 0);
}
```

```c:games/my-game/src/inputs.h
#ifndef MY_GAME_INPUTS_H
#define MY_GAME_INPUTS_H

#include "../../../engine/game/inputs-base.h"

#define DE100_GAME_BUTTON_COUNT 4

#define DE100_GAME_BUTTON_FIELDS \
    GameButtonState move_up;     \
    GameButtonState move_down;   \
    GameButtonState move_left;   \
    GameButtonState move_right;

#endif
```

### 3. Create Build Script

```bash:games/my-game/build-dev.sh
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

source "$PROJECT_ROOT/engine/build-common.sh"

# Set game-specific variables
GAME_NAME="my-game"
GAME_DIR="$SCRIPT_DIR"

# Build
de100_build_game "$GAME_NAME" "$GAME_DIR"
```

### 4. Build and Run

```bash
cd games/my-game
chmod +x build-dev.sh
./build-dev.sh
./build/my-game
```

## Private Submodules

Some games (like `handmade-hero`) are private submodules due to:

- Licensing restrictions
- Disk space considerations
- Commercial content

### Requesting Access

Contact the repository owner if you need access to a private game submodule.

### Working Without Access

The engine is fully functional without private game submodules. You can:

1. Create your own game (see above)
2. Contribute to engine code
3. Use the public example game (if available)

````

---

## Step 5: Handle Existing Code References

### 5.1 Update Include Paths (If Needed)

Since the directory path stays the same (`games/handmade-hero/`), include paths should work unchanged. However, verify:

```bash
# Check for any hardcoded absolute paths
grep -r "/home/.*games/handmade-hero" .
grep -r "C:\\\\.*games\\\\handmade-hero" .
````

### 5.2 Update CI/CD Configuration

```yaml:.github/workflows/build.yml
name: Build

on: [push, pull_request]

jobs:
  build-engine:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        # Don't checkout submodules for engine-only build

      - name: Build Engine
        run: |
          cd engine
          ./build-engine-only.sh

  build-with-games:
    runs-on: ubuntu-latest
    # Only run if we have access to private repos
    if: github.event_name == 'push' && github.repository == 'your-username/de100-engine'
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
          # Use deploy key or PAT for private submodule access
          ssh-key: ${{ secrets.SUBMODULE_SSH_KEY }}

      - name: Build All Games
        run: |
          for game_dir in games/*/; do
            if [[ -f "$game_dir/build-dev.sh" ]]; then
              echo "Building $(basename $game_dir)..."
              cd "$game_dir"
              ./build-dev.sh
              cd ../..
            fi
          done
```

### 5.3 Create SSH Deploy Key for CI

```bash
# Generate deploy key for the private submodule
ssh-keygen -t ed25519 -C "ci-deploy-key" -f ./deploy_key -N ""

# Add deploy_key.pub to private repo's deploy keys (read-only)
# Add deploy_key (private) to main repo's secrets as SUBMODULE_SSH_KEY
```

---

## Step 6: Development Workflow

### 6.1 Daily Workflow Script

Create a helper script for developers:

```bash:scripts/dev-setup.sh
#!/bin/bash
set -e

echo "═══════════════════════════════════════════════════════════════════"
echo "DE100 Engine Development Setup"
echo "═══════════════════════════════════════════════════════════════════"

# Check if submodules are configured
if [[ -f ".gitmodules" ]]; then
    echo ""
    echo "Checking submodules..."

    # Try to initialize submodules
    if git submodule update --init --recursive 2>/dev/null; then
        echo "✅ All submodules initialized successfully"
    else
        echo ""
        echo "⚠️  Some submodules could not be initialized."
        echo "   This is normal if you don't have access to private repos."
        echo ""
        echo "   You can still:"
        echo "   - Build and modify the engine"
        echo "   - Create your own game in games/your-game-name/"
        echo ""
    fi
fi

echo ""
echo "Setup complete! Next steps:"
echo ""
echo "  1. Build the engine:"
echo "     cd engine && ./build-engine-only.sh"
echo ""
echo "  2. Create or build a game:"
echo "     cd games/your-game && ./build-dev.sh"
echo ""
echo "═══════════════════════════════════════════════════════════════════"
```

### 6.2 Submodule Update Workflow

```bash:scripts/update-submodules.sh
#!/bin/bash
set -e

echo "Updating all submodules to latest..."

# Update all submodules
git submodule update --remote --merge

# Show status
echo ""
echo "Submodule status:"
git submodule status

echo ""
echo "If you want to commit these updates:"
echo "  git add games/handmade-hero"
echo "  git commit -m 'chore: update handmade-hero submodule'"
```

---

## Step 7: Push/Pull Workflow for Submodule

### 7.1 Making Changes to the Submodule

```bash
# Navigate to submodule
cd games/handmade-hero

# Make changes
vim src/main.c

# Commit in submodule
git add .
git commit -m "feat: add new feature"

# Push submodule changes
git push origin main

# Go back to main repo
cd ../..

# Update main repo's submodule reference
git add games/handmade-hero
git commit -m "chore: update handmade-hero submodule reference"
git push
```

### 7.2 Pulling Changes

```bash
# Pull main repo
git pull

# Update submodules to match recorded commits
git submodule update --recursive

# OR: Update submodules to latest remote
git submodule update --remote --recursive
```

### 7.3 Helper Aliases

Add to your `.gitconfig`:

```ini
[alias]
    # Clone with submodules
    clone-full = clone --recursive

    # Pull with submodule update
    pull-all = !git pull && git submodule update --recursive

    # Update submodules to latest
    sub-update = submodule update --remote --recursive

    # Push submodule then main repo
    push-all = !git submodule foreach 'git push origin HEAD:main || true' && git push
```

---

## Step 8: Managing Multiple Game Submodules

### 8.1 Submodule Registry Pattern

Create a configuration file for managing multiple games:

```yaml:games/games.yaml
# Game Submodule Registry
# This file documents all game submodules and their access requirements

games:
  handmade-hero:
    path: games/handmade-hero
    url: git@github.com:your-username/handmade-hero.git
    access: private
    description: "Handmade Hero implementation following Casey Muratori's series"
    maintainers:
      - your-username

  # Future games can be added here
  # puzzle-game:
  #   path: games/puzzle-game
  #   url: git@github.com:your-username/puzzle-game.git
  #   access: private
  #   description: "A puzzle game built with DE100 engine"

  example-game:
    path: games/example-game
    access: public
    description: "Public example game demonstrating engine features"
```

### 8.2 Multi-Submodule Management Script

```bash:scripts/manage-games.sh
#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

show_help() {
    cat << EOF
DE100 Game Submodule Manager

Usage: $0 <command> [options]

Commands:
    list        List all game submodules and their status
    init        Initialize all accessible submodules
    add         Add a new game submodule
    remove      Remove a game submodule
    status      Show detailed status of all games

Examples:
    $0 list
    $0 init
    $0 add my-new-game git@github.com:user/my-new-game.git
    $0 remove old-game
EOF
}

list_games() {
    echo "═══════════════════════════════════════════════════════════════════"
    echo "Game Submodules"
    echo "═══════════════════════════════════════════════════════════════════"
    echo ""

    if [[ ! -f "$PROJECT_ROOT/.gitmodules" ]]; then
        echo "No submodules configured."
        return
    fi

    printf "%-25s %-15s %s\n" "GAME" "STATUS" "PATH"
    printf "%-25s %-15s %s\n" "----" "------" "----"

    while IFS= read -r line; do
        if [[ "$line" =~ path\ =\ (games/[^[:space:]]+) ]]; then
            local path="${BASH_REMATCH[1]}"
            local name="$(basename "$path")"
            local status="❓ Unknown"

            if [[ -d "$PROJECT_ROOT/$path/.git" ]] || [[ -f "$PROJECT_ROOT/$path/.git" ]]; then
                if [[ -f "$PROJECT_ROOT/$path/src/main.c" ]]; then
                    status="✅ Ready"
                else
                    status="⚠️  Empty"
                fi
            else
                status="❌ Not initialized"
            fi

            printf "%-25s %-15s %s\n" "$name" "$status" "$path"
        fi
    done < "$PROJECT_ROOT/.gitmodules"

    echo ""
}

init_games() {
    echo "Initializing game submodules..."
    echo ""

    cd "$PROJECT_ROOT"

    # Try to init each submodule individually
    git config --file .gitmodules --get-regexp path | while read -r key path; do
        if [[ "$path" == games/* ]]; then
            local name="$(basename "$path")"
            echo -n "  $name: "

            if git submodule update --init "$path" 2>/dev/null; then
                echo "✅ Initialized"
            else
                echo "❌ Failed (no access?)"
            fi
        fi
    done

    echo ""
    echo "Done. Run '$0 list' to see status."
}

add_game() {
    local name="$1"
    local url="$2"

    if [[ -z "$name" ]] || [[ -z "$url" ]]; then
        echo "Usage: $0 add <game-name> <git-url>"
        exit 1
    fi

    local path="games/$name"

    if [[ -d "$PROJECT_ROOT/$path" ]]; then
        echo "Error: $path already exists"
        exit 1
    fi

    echo "Adding game submodule: $name"
    cd "$PROJECT_ROOT"
    git submodule add "$url" "$path"

    echo ""
    echo "✅ Submodule added. Don't forget to:"
    echo "   git commit -m 'chore: add $name game submodule'"
}

remove_game() {
    local name="$1"

    if [[ -z "$name" ]]; then
        echo "Usage: $0 remove <game-name>"
        exit 1
    fi

    local path="games/$name"

    if [[ ! -d "$PROJECT_ROOT/$path" ]]; then
        echo "Error: $path does not exist"
        exit 1
    fi

    echo "⚠️  This will remove the submodule: $name"
    read -p "Are you sure? (y/N) " -n 1 -r
    echo

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        cd "$PROJECT_ROOT"
        git submodule deinit -f "$path"
        git rm -f "$path"
        rm -rf ".git/modules/$path"

        echo "✅ Submodule removed. Don't forget to:"
        echo "   git commit -m 'chore: remove $name game submodule'"
    else
        echo "Cancelled."
    fi
}

case "${1:-}" in
    list)
        list_games
        ;;
    init)
        init_games
        ;;
    add)
        add_game "$2" "$3"
        ;;
    remove)
        remove_game "$2"
        ;;
    status)
        list_games
        ;;
    -h|--help|help)
        show_help
        ;;
    *)
        show_help
        exit 1
        ;;
esac
```

---

## Step 9: .gitmodules Configuration

Your `.gitmodules` file will look like this:

```ini:.gitmodules
[submodule "games/handmade-hero"]
	path = games/handmade-hero
	url = git@github.com:your-username/handmade-hero.git
	# Optional: specify branch
	branch = main

# Future submodules:
# [submodule "games/puzzle-game"]
# 	path = games/puzzle-game
# 	url = git@github.com:your-username/puzzle-game.git
# 	branch = main
```

---

## Step 10: Contributor Communication

### 10.1 CONTRIBUTING.md Update

````markdown:CONTRIBUTING.md
# Contributing to DE100 Engine

## Repository Structure

This repository uses **git submodules** for game projects. The engine itself is public, but some games are private.

## Getting Started

### Clone the Repository

```bash
# Basic clone (engine only)
git clone https://github.com/your-username/de100-engine.git

# Full clone with submodules (if you have access)
git clone --recursive git@github.com:your-username/de100-engine.git
````

### Submodule Access

| Component              | Access     | How to Contribute  |
| ---------------------- | ---------- | ------------------ |
| `engine/`              | 🌍 Public  | PRs welcome!       |
| `games/handmade-hero/` | 🔒 Private | Contact maintainer |
| `games/example-game/`  | 🌍 Public  | PRs welcome!       |

### If You Don't Have Submodule Access

**That's okay!** You can still:

1. **Contribute to the engine** - All engine code is public
2. **Create your own game** - See `games/README.md`
3. **Use the example game** - A public game for testing

### Working with Submodules

If you have access to private submodules:

```bash
# After cloning, initialize submodules
git submodule update --init --recursive

# Pull latest changes including submodules
git pull && git submodule update --recursive

# Update submodules to latest remote
git submodule update --remote --recursive
```

### Making Changes to a Submodule

```bash
# 1. Navigate to submodule
cd games/handmade-hero

# 2. Create branch, make changes, commit
git checkout -b feature/my-feature
# ... make changes ...
git commit -m "feat: my feature"

# 3. Push submodule changes
git push origin feature/my-feature

# 4. Create PR in submodule repo

# 5. After merge, update main repo
cd ../..
git add games/handmade-hero
git commit -m "chore: update handmade-hero submodule"
git push
```

```

---

## Summary Checklist

- [ ] Create private repository for `handmade-hero`
- [ ] Extract git history (optional)
- [ ] Remove directory from main repo
- [ ] Add as submodule
- [ ] Update `build-common.sh` with submodule checks
- [ ] Update game build scripts
- [ ] Create `games/README.md` documentation
- [ ] Update main `README.md`
- [ ] Update `CONTRIBUTING.md`
- [ ] Configure CI/CD with deploy keys
- [ ] Create helper scripts (`dev-setup.sh`, `manage-games.sh`)
- [ ] Test clone workflow (with and without access)
- [ ] Communicate changes to existing contributors
```
