#include <algorithm>
#include <random>
#include <iostream>
#include <climits>
using namespace std;;

#include "board.hpp"
#include "engine.hpp"

const int PAWN_WEIGHT = 100;
const int ROOK_WEIGHT = 300;
const int BISHOP_WEIGHT = 500;
const int KING_WEIGHT = 1000000;
const int CHECK_WEIGHT = 300;

void flip_player(Board& b) {
    b.data.player_to_play = (PlayerColor)(b.data.player_to_play ^ (WHITE | BLACK));
}


int eval(Board& b) {
    int score = 0;
    U8 white_pieces[6] = {b.data.w_rook_ws, b.data.w_rook_bs, b.data.w_king, b.data.w_bishop, b.data.w_pawn_ws, b.data.w_pawn_bs};
    U8 black_pieces[6] = {b.data.b_rook_ws, b.data.b_rook_bs, b.data.b_king, b.data.b_bishop, b.data.b_pawn_ws, b.data.b_pawn_bs};
    int weights[6] = {ROOK_WEIGHT, ROOK_WEIGHT, KING_WEIGHT, BISHOP_WEIGHT, PAWN_WEIGHT, PAWN_WEIGHT};

    for (int i = 0; i < 6; i++) {
        if (white_pieces[i] != DEAD) {
            score += weights[i];
        }
        if (black_pieces[i] != DEAD) {
            score -= weights[i];
        }
    }

    flip_player(b);

    unordered_set<U16> opponent_moves = b.get_legal_moves();

    for (auto move : opponent_moves) {
        U8 final_pos = getp1(move);
        for (int i = 0; i < 6; i++) {
            if (final_pos == white_pieces[i]) {
                score -= weights[i] / 6;
            }
        }
    }

    if (b.in_check()) {
        score += CHECK_WEIGHT;
    }

    flip_player(b);

    return score;
}

bool check_equal_boards(Board& b1, Board& b2) {
    bool equal = true;
    if (b1.data.b_king == b2.data.b_king) {
        equal = false;
    }
    if (b1.data.b_bishop == b2.data.b_bishop) {
        equal = false;
    }
    if (b1.data.b_pawn_bs == b2.data.b_pawn_bs) {
        equal = false;
    }
    if (b1.data.b_pawn_ws == b2.data.b_pawn_ws) {
        equal = false;
    }
    if (b1.data.b_rook_bs == b2.data.b_rook_bs) {
        equal = false;
    }
    if (b1.data.b_rook_ws == b2.data.b_rook_ws) {
        equal = false;
    }
    if (b1.data.w_king == b2.data.w_king) {
        equal = false;
    }
    if (b1.data.w_bishop == b2.data.w_bishop) {
        equal = false;
    }
    if (b1.data.w_pawn_bs == b2.data.w_pawn_bs) {
        equal = false;
    }
    if (b1.data.w_pawn_ws == b2.data.w_pawn_ws) {
        equal = false;
    }
    if (b1.data.w_rook_bs == b2.data.w_rook_bs) {
        equal = false;
    }
    if (b1.data.w_rook_ws == b2.data.w_rook_ws) {
        equal = false;
    }
    return equal;
}


int minimax(Board& board, int depth, bool maximizingPlayer, vector<Board*> &visited) {
    if (depth == 0) {
        return eval(board);
    }

    if (maximizingPlayer) {
        int maxEval = INT_MIN;
        for (auto move : board.get_legal_moves()) {
            // Apply the move to a copy of the board
            Board* newBoard = board.copy();
            newBoard->do_move(move);
            bool flag = false;
            for (Board* b : visited) {
                if (check_equal_boards(*b, *newBoard)) {
                    flag = true;
                    break;
                }
            }
            if (flag) {
                continue;
            }
            visited.push_back(newBoard);
            int eval = minimax(*newBoard, depth - 1, false, visited);
            visited.pop_back();
            maxEval = max(maxEval, eval);
        }
        return maxEval;
    } else {
        int minEval = INT_MAX;
        for (auto move : board.get_legal_moves()) {
            // Apply the move to a copy of the board
            Board* newBoard = board.copy();
            newBoard->do_move(move);
            bool flag = false;
            for (Board* b : visited) {
                if (check_equal_boards(*b, *newBoard)) {
                    flag = true;
                    break;
                }
            }
            if (flag) {
                continue;
            }
            visited.push_back(newBoard);
            int eval = minimax(*newBoard, depth - 1, true, visited);
            visited.pop_back();
            minEval = min(minEval, eval);
        }
        return minEval;
    }
}

// U16 find_best_move(const Board& board, int depth) {
//     int bestEval = INT_MIN;
//     U16 bestMove = 0;

//     for (auto move : board.get_legal_moves()) {
        
//         Board* newBoard = board.copy();
//         newBoard->do_move(move);
//         int eval = minimax(*newBoard, depth - 1, false);

//         if (eval > bestEval) {
//             bestEval = eval;
//             bestMove = move;
//         }
//     }

//     return bestMove;
// }





void Engine::find_best_move(const Board& b) {
    int depth = 2;
    int bestEval = INT_MIN;
    int bestMove=0;
     


    const unordered_set<U16> vec = b.get_legal_moves();
    for(auto x: vec){
        cout<<x<<endl;
    }

    
    auto moveset = b.get_legal_moves();
    if (moveset.size() == 0) {
        this->best_move = 0;
    }
    else {
        std::vector<U16> moves;
        std::cout << all_boards_to_str(b) << std::endl;
        for (auto m : moveset) {
            std::cout << move_to_str(m) << " ";
        }
        std::cout << std::endl;
        std::sample(
            moveset.begin(),
            moveset.end(),
            std::back_inserter(moves),
            1,
            std::mt19937{std::random_device{}()}
        );
        for (auto move : b.get_legal_moves()) {
            // Apply the move to a copy of the board
            Board* newBoard = b.copy();
            newBoard->do_move(move);
            vector<Board*> visited;
            visited.push_back(newBoard);
            int eval = minimax(*newBoard, depth - 1, false, visited);

            if (eval > bestEval) {
                bestEval = eval;
                bestMove = move;
                this->best_move = move;
            }
        }

    
        this->best_move = bestMove;
    }
}
